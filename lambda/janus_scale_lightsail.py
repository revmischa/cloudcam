import json
import logging
import os
import random
import time
import boto3
from cloudcam.tools import rand_string

logger = logging.getLogger()
logger.setLevel(logging.INFO)

lightsail_azs = [x.strip() for x in os.environ['LIGHTSAIL_AZS'].split(',')]
lightsail_blueprint_id = os.environ['LIGHTSAIL_BLUEPRINT_ID']
lightsail_bundle_id = os.environ['LIGHTSAIL_BUNDLE_ID']
lightsail_janus_image = os.environ['LIGHTSAIL_JANUS_IMAGE']
janus_hosted_zone_id = os.environ['JANUS_HOSTED_ZONE_ID']
janus_hosted_zone_domain = os.environ['JANUS_HOSTED_ZONE_DOMAIN']
janus_instance_name_prefix = os.environ['JANUS_INSTANCE_NAME_PREFIX']

lightsail_init_script = """
yum update -y
yum install -y docker
mkdir /etc/docker
cat >/etc/docker/daemon.json << EOL
{{
    "userland-proxy": false
}}
EOL
service docker start
usermod -a -G docker ec2-user
docker run -d --name docker-janus --restart=always -p 8080:8080 -p 8088:8088 -p 8089:8089 -p 7889:7889 -p 8188:8188 -p 10000-10200:10000-10200/udp -p 20000-21000:20000-21000/udp {0}
""".format(lightsail_janus_image)

lightsail = boto3.client('lightsail')
route53 = boto3.client('route53')


def get_janus_instances():
    """Returns a list of Janus instances available for stream allocation"""
    return list(filter(lambda instance: instance['name'].startswith(janus_instance_name_prefix),
                       lightsail.get_instances()['instances']))


def open_instance_public_tcp_port(instance_name, port):
    lightsail.open_instance_public_ports(instanceName=instance_name,
                                         portInfo={
                                             'fromPort': port,
                                             'toPort': port,
                                             'protocol': 'tcp'
                                         })


def open_instance_public_udp_port_range(instance_name, from_port, to_port):
    lightsail.open_instance_public_ports(instanceName=instance_name,
                                         portInfo={
                                             'fromPort': from_port,
                                             'toPort': to_port,
                                             'protocol': 'udp'
                                         })


def get_instance_status(instance_name):
    instance = lightsail.get_instance(instanceName=instance_name)
    return instance['instance']['state']['name']


def create_janus_instance():
    """Creates a Lightsail instance running Janus container"""
    # randomly select an availability zone if multiple ones are specified
    lightsail_az = random.choice(lightsail_azs)
    instance_name = f'{janus_instance_name_prefix}-{lightsail_az}-{rand_string(12)}'.lower()
    logger.info(
        f'Creating instance: {instance_name}, {lightsail_az}, {lightsail_blueprint_id}, {lightsail_bundle_id}, {lightsail_init_script}')
    # create/start the instance (Janus docker container will be set up/started via lightsail_init_script)
    lightsail.create_instances(instanceNames=[instance_name],
                               availabilityZone=lightsail_az,
                               blueprintId=lightsail_blueprint_id,
                               bundleId=lightsail_bundle_id,
                               userData=lightsail_init_script)
    # wait until the instance is running
    for i in range(120):
        time.sleep(1)
        if get_instance_status(instance_name) == 'running':
            break
    # open ports requred for Janus
    for port in [8080, 8088, 8089, 7889, 8188]:
        open_instance_public_tcp_port(instance_name, port)
    open_instance_public_udp_port_range(instance_name, 10000, 10200)
    open_instance_public_udp_port_range(instance_name, 20000, 21000)
    # get instance ip addresses
    instance = lightsail.get_instance(instanceName=instance_name)
    ipv4_address = instance['instance']['publicIpAddress']
    domain_name = f'{instance_name}.{janus_hosted_zone_domain}'
    # create dns alias for the instance
    route53.change_resource_record_sets(
        HostedZoneId=janus_hosted_zone_id,
        ChangeBatch={
            'Changes': [
                {
                    'Action': 'UPSERT',
                    'ResourceRecordSet': {
                        'Name': domain_name,
                        'Type': 'A',
                        'TTL': 600,
                        'ResourceRecords': [
                            {
                                'Value': ipv4_address
                            }
                        ]
                    }
                }
            ]
        })
    # todo: pass ssl cert into the container
    return {'name': instance_name}


def remove_janus_instance(instance):
    """Removes a Lightsail instance"""
    # get instance ip addresses
    instance_name = instance['name']
    instance = lightsail.get_instance(instanceName=instance_name)
    ipv4_address = instance['instance']['publicIpAddress']
    domain_name = f'{instance_name}.{janus_hosted_zone_domain}'
    # remove the dns alias
    route53.change_resource_record_sets(
        HostedZoneId=janus_hosted_zone_id,
        ChangeBatch={
            'Changes': [
                {
                    'Action': 'DELETE',
                    'ResourceRecordSet': {
                        'Name': domain_name,
                        'Type': 'A',
                        'TTL': 600,
                        'ResourceRecords': [
                            {
                                'Value': ipv4_address
                            }
                        ]
                    }
                }
            ]
        })
    # delete the instance
    lightsail.delete_instance(instanceName=instance_name)


# todo: check/purge dead instances
# todo: rolling image upgrades
def handler(event, context):
    """Ensures the specified number of Janus container instances are running on Hyper.sh"""
    logger.info(json.dumps(event, sort_keys=True, indent=4))

    # lambda parameters
    required_instance_count = event['requiredInstanceCount']

    instances = get_janus_instances()
    current_instance_count = len(instances)

    logger.info(f'Janus instance required count: {required_instance_count}, current count: {current_instance_count}')

    if current_instance_count > required_instance_count:
        for _ in range(current_instance_count - required_instance_count):
            logger.info(f'Removing Janus instance {instances[-1]}')
            remove_janus_instance(instances[-1])
            instances.pop()
    elif current_instance_count < required_instance_count:
        for _ in range(required_instance_count - current_instance_count):
            instance = create_janus_instance()
            logger.info(f'Created Janus instance {instance}')
