import json
import logging
import random
import string
from time import time

import boto3
import requests

logger = logging.getLogger()
logger.setLevel(logging.INFO)

cluster_name_prefix = 'cloudcamdev-janus-ecs'
janus_verify_https = False
janus_connect_timeout = 5
janus_rtp_port_range = range(20000, 20046, 2)

ecs = boto3.client('ecs')
ec2 = boto3.client('ec2')

iot = boto3.client('iot')
iot_data = boto3.client('iot-data')


def get_janus_instances():
    ec2_instance_ids = []
    cluster_arns = ecs.list_clusters()['clusterArns']
    for cluster_arn in cluster_arns:
        logger.info(cluster_arn.split(':'))
        logger.info(cluster_arn.split(':')[-1])
        if cluster_arn.split(':')[-1].startswith('cluster/' + cluster_name_prefix):
            logger.info(f'janus cluster arn: {cluster_arn}')
            container_instance_arns = ecs.list_container_instances(
                cluster=cluster_arn,
                status='ACTIVE')['containerInstanceArns']
            logger.info(f'janus container instance arns: {container_instance_arns}')
            container_instances = ecs.describe_container_instances(
                cluster=cluster_arn,
                containerInstances=container_instance_arns)['containerInstances']
            logger.info(f'janus container instances: {container_instances}')
            for container_instance in container_instances:
                ec2_instance_ids.append(container_instance['ec2InstanceId'])
    ec2_instance_info = ec2.describe_instances(InstanceIds=ec2_instance_ids)
    janus_ec2_instances = []
    for reservation in ec2_instance_info['Reservations']:
        for instance in reservation['Instances']:
            if instance['State']['Name'] == 'running':
                janus_ec2_instances.append(instance)
    return janus_ec2_instances


def rand_string(size=12, chars=string.ascii_uppercase + string.ascii_uppercase + string.digits):
    return ''.join(random.choice(chars) for _ in range(size))


def janus_create_session(s, url):
    response = s.post(url, data=json.dumps({'janus': 'create', 'transaction': rand_string()}),
                      verify=janus_verify_https, timeout=janus_connect_timeout).json()
    if response['janus'] != 'success':
        raise RuntimeError(f'Janus response error: {response}')
    return url + '/' + str(response['data']['id'])


def janus_attach_plugin(s, session_url, plugin):
    response = s.post(session_url, data=json.dumps({'janus': 'attach',
                                                    'plugin': plugin,
                                                    'transaction': rand_string()}),
                      verify=janus_verify_https, timeout=janus_connect_timeout).json()
    if response['janus'] != 'success':
        raise RuntimeError(f'Janus response error: {response}')
    return session_url + '/' + str(response['data']['id'])


def janus_send_plugin_message(s, plugin_url, body):
    response = s.post(plugin_url, data=json.dumps({'janus': 'message',
                                                   'body': body,
                                                   'transaction': rand_string()}),
                      verify=janus_verify_https, timeout=janus_connect_timeout).json()
    if response['janus'] != 'success':
        raise RuntimeError(f'Janus response error: {response}')
    return response['plugindata']['data']


def janus_allocate_stream(janus_gateway_dns_name, stream):
    janus_rest_url = f'https://{janus_gateway_dns_name}:8089/janus'
    logger.info(janus_rest_url)

    s = requests.Session()

    session_url = janus_create_session(s, janus_rest_url)
    logger.info(session_url)

    streaming_plugin_url = janus_attach_plugin(s, session_url, 'janus.plugin.streaming')
    logger.info(streaming_plugin_url)

    streams = janus_send_plugin_message(s, streaming_plugin_url, {'request': 'list'})['list']
    logger.info(streams)

    rtp_port = None
    stream_id = None
    stream_name = None
    stream_secret = None
    stream_pin = None

    # check if our stream is already allocated
    if stream and stream['stream_name']:
        for st in streams:
            if st.get('id') == stream['stream_id'] and st.get('description') == stream['stream_name']:
                rtp_port = stream['stream_rtp_port']
                stream_id = stream['stream_id']
                stream_name = stream['stream_name']
                stream_secret = stream['stream_secret']
                stream_pin = stream['stream_pin']

    # otherwise, allocate new RTP port/stream name/secret/pin
    if not (rtp_port or stream_id or stream_name or stream_secret or stream_pin):
        used_rtp_ports = set([o['id'] for o in streams])
        rtp_port = None
        for p in janus_rtp_port_range:
            if p not in used_rtp_ports:
                rtp_port = p
                break

        if not rtp_port:
            raise RuntimeError("Unable to allocate RTP port")

        stream_id = rtp_port
        stream_name = f'rtp-h264-{stream_id}-{rand_string(size=12)}'
        stream_secret = rand_string(size=42)
        stream_pin = rand_string(size=42)

        logger.info(f'allocated RTP port {rtp_port} for stream {stream_name}')

        data = janus_send_plugin_message(s, streaming_plugin_url,
                                         {'request': 'create',
                                          'type': 'rtp',
                                          # 'secret': stream_secret,
                                          # 'pin': stream_pin,
                                          'permanent': False,
                                          'id': stream_id,
                                          'name': stream_name,
                                          'is_private': False,
                                          'video': True,
                                          'audio': False,
                                          'videoport': rtp_port,
                                          'videopt': 96,
                                          'videortpmap': 'H264/90000',
                                          'videofmtp': 'profile-level-id=42e028;packetization-mode=1'})
        logger.info(data)

        if data.get('error') or data.get('error_code'):
            raise RuntimeError(f'Janus error {data.get("error_code")}: {data.get("error")}')

        streams = janus_send_plugin_message(s, streaming_plugin_url, {'request': 'list'})['list']
        logger.info(streams)

    return {'gateway_instance': janus_gateway_dns_name,
            'gateway_url': janus_rest_url,
            'session_url': session_url,
            'streaming_plugin_url': streaming_plugin_url,
            'stream_id': stream_id,
            'stream_name': stream_name,
            'stream_secret': stream_secret,
            'stream_pin': stream_pin,
            'stream_rtp_port': rtp_port,
            'stream_h264_bitrate': 256,
            'stream_enable': True,
            'timestamp': int(time())}


def handler(event, context):
    logger.info(json.dumps(event, sort_keys=True, indent=4))

    thing_name = event['thingName']

    logger.info(thing_name)

    thing_shadow = json.loads(iot_data.get_thing_shadow(thingName=thing_name)['payload'].read().decode('utf-8'))
    logger.info(json.dumps(thing_shadow, indent=2))

    streams = thing_shadow['state']['desired'].get('streams') or {}

    janus_instances = get_janus_instances()
    for instance in janus_instances:
        instance_id = instance['InstanceId']
        instance_type = instance['InstanceType']
        private_dns_name = instance['PrivateDnsName']
        public_dns_name = instance['PublicDnsName']
        logger.info(f'janus container instance {instance_id} {instance_type} {private_dns_name} {public_dns_name}')

    primary_gateway = random.choice(janus_instances)
    primary_gateway_dns_name = primary_gateway["PublicDnsName"]

    standby_janus_instances = janus_instances[:].remove(primary_gateway)
    standby_gateway_dns_name = standby_janus_instances and random.choice(standby_janus_instances)["PublicDnsName"]

    logger.info(f'gateway: {primary_gateway_dns_name}, standby gateway: {standby_gateway_dns_name}')

    # todo: retry with another gateway on failure
    primary_stream = janus_allocate_stream(primary_gateway_dns_name, streams.get('primary'))
    standby_stream = standby_gateway_dns_name and janus_allocate_stream(standby_gateway_dns_name,
                                                                        streams.get('standby'))
    current_stream = streams.get('current', 'primary')

    iot_data.update_thing_shadow(
        thingName=thing_name,
        payload=json.dumps({'state': {
            'desired': {
                'streams': {
                    'primary': primary_stream,
                    'standby': standby_stream,
                    'current': current_stream
                }
            }
        }}).encode('utf-8'))

    return {
        'primary': primary_stream,
        'standby': standby_stream,
        'current': current_stream
    }
