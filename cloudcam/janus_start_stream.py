import json
import logging
import random
from time import time
import os
from cloudcam.tools import rand_string

import boto3
import requests

logger = logging.getLogger()
logger.setLevel(logging.INFO)

janus_verify_https = False
janus_connect_timeout = 5
janus_rtp_port_range = range(20000, 21000, 2)
janus_hosted_zone_domain = os.environ['JANUS_HOSTED_ZONE_DOMAIN']
janus_instance_name_prefix = os.environ['JANUS_INSTANCE_NAME_PREFIX']

lightsail = boto3.client('lightsail')

iot = boto3.client('iot')
iot_data = boto3.client('iot-data')


def get_lightsail_public_dns_name(instance):
    return f'{instance["name"]}.{janus_hosted_zone_domain}'


def translate_lightsail_instance(instance):
    return {'instance_id': instance['name'],
            'public_dns_name': get_lightsail_public_dns_name(instance)}


def get_janus_instances():
    """Returns a list of Janus instances available for stream allocation (AWS Lightsail)"""
    return list(map(translate_lightsail_instance,
                    filter(lambda instance: instance['name'].startswith(janus_instance_name_prefix),
                           lightsail.get_instances()['instances'])))


def janus_create_session(s, url):
    """Creates a session via Janus REST API"""
    response = s.post(url, data=json.dumps({'janus': 'create', 'transaction': rand_string()}),
                      verify=janus_verify_https, timeout=janus_connect_timeout).json()
    if response['janus'] != 'success':
        raise RuntimeError(f'Janus response error: {response}')
    return url + '/' + str(response['data']['id'])


def janus_attach_plugin(s, session_url, plugin):
    """Attaches a plugin to an existing session via Janus REST API"""
    response = s.post(session_url, data=json.dumps({'janus': 'attach',
                                                    'plugin': plugin,
                                                    'transaction': rand_string()}),
                      verify=janus_verify_https, timeout=janus_connect_timeout).json()
    if response['janus'] != 'success':
        raise RuntimeError(f'Janus response error: {response}')
    return session_url + '/' + str(response['data']['id'])


def janus_send_plugin_message(s, plugin_url, body):
    """Sends a message to plugin attached to an existing session via Janus REST API"""
    response = s.post(plugin_url, data=json.dumps({'janus': 'message',
                                                   'body': body,
                                                   'transaction': rand_string()}),
                      verify=janus_verify_https, timeout=janus_connect_timeout).json()
    if response['janus'] != 'success':
        raise RuntimeError(f'Janus response error: {response}')
    return response['plugindata']['data']


def janus_allocate_stream(janus_gateway_dns_name, stream):
    """Allocates a RTP input stream on a specified Janus instance via Janus REST API"""
    janus_rest_url = f'https://{janus_gateway_dns_name}:8089/janus'
    logger.info(janus_rest_url)

    # use a requests session so https connection persists across requests
    s = requests.Session()

    # create Janus session
    session_url = janus_create_session(s, janus_rest_url)
    logger.info(session_url)

    # attach streaming plugin to the session
    streaming_plugin_url = janus_attach_plugin(s, session_url, 'janus.plugin.streaming')
    logger.info(streaming_plugin_url)

    # list existing streams
    streams = janus_send_plugin_message(s, streaming_plugin_url, {'request': 'list'})['list']
    logger.info(streams)

    rtp_port = None
    stream_id = None
    stream_name = None
    stream_secret = None
    stream_pin = None

    # check if there's a stream already allocated (this data is stored in the iot thing shadow)
    if stream and stream['stream_name']:
        for st in streams:
            if st.get('id') == stream['stream_id'] and st.get('description') == stream['stream_name']:
                rtp_port = stream['stream_rtp_port']
                stream_id = stream['stream_id']
                stream_name = stream['stream_name']
                stream_secret = stream['stream_secret']
                stream_pin = stream['stream_pin']

    # otherwise, find a free port pair for RTP input and create the stream
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

        # this creates the stream via a messagee to Janus streaming plugin
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
            'stream_h264_bitrate': 256,  # todo: make bitrate adjustable
            'stream_enable': True,
            'timestamp': int(time())}


def handler(event, context):
    """Creates/starts a RTP stream from the thing"""
    logger.info(json.dumps(event, sort_keys=True, indent=4))

    # lambda parameters
    thing_name = event['thingName']
    logger.info(thing_name)

    # retrieve iot thing shadow document
    thing_shadow = json.loads(iot_data.get_thing_shadow(thingName=thing_name)['payload'].read().decode('utf-8'))
    logger.info(json.dumps(thing_shadow, indent=2))

    # retrieve currently allocated stream info
    streams = thing_shadow['state'].get('desired', {}).get('streams') or {}

    # get a list of available Janus instances
    janus_instances = get_janus_instances()
    for instance in janus_instances:
        instance_id = instance['instance_id']
        public_dns_name = instance['public_dns_name']
        logger.info(f'janus container instance {instance_id} {public_dns_name}')

    # allocate primary/standby Janus instances
    primary_gateway = random.choice(janus_instances)
    primary_gateway_dns_name = primary_gateway["public_dns_name"]

    standby_janus_instances = janus_instances[:].remove(primary_gateway)
    standby_gateway_dns_name = standby_janus_instances and random.choice(standby_janus_instances)['public_dns_name']

    logger.info(f'gateway: {primary_gateway_dns_name}, standby gateway: {standby_gateway_dns_name}')

    # todo: retry with another gateway on failure
    primary_stream = janus_allocate_stream(primary_gateway_dns_name, streams.get('primary'))
    standby_stream = standby_gateway_dns_name and janus_allocate_stream(standby_gateway_dns_name,
                                                                        streams.get('standby'))
    current_stream = streams.get('current', 'primary')

    # update iot thing shadow with new stream data
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
