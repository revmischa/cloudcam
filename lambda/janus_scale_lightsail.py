import json
import logging
import os
import random
import time
import boto3
import base64
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

# Janus SSL cert data
janus_ssl_cert_pem = """-----BEGIN CERTIFICATE-----
MIIG2jCCBcKgAwIBAgIMBGVh/DUTLawsXJ24MA0GCSqGSIb3DQEBCwUAMEwxCzAJ
BgNVBAYTAkJFMRkwFwYDVQQKExBHbG9iYWxTaWduIG52LXNhMSIwIAYDVQQDExlB
bHBoYVNTTCBDQSAtIFNIQTI1NiAtIEcyMB4XDTE3MDkxMzEyMjczMFoXDTE4MDkx
NDEyMjczMFowPjEhMB8GA1UECxMYRG9tYWluIENvbnRyb2wgVmFsaWRhdGVkMRkw
FwYDVQQDDBAqLmNsb3VkY2FtLnNwYWNlMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8A
MIIBCgKCAQEA7Yc26Af305J+MSPQ4El0Rn6ZV8H4y1E1c9DenKwTp+Gb/FbTZC0J
ztY04Tz1cJIasZwie6CVL47jQFHmEL3KBWAahTRwT8atjl40SKWWIugdoLpxb2d+
s6HV1JA3G3aGSBIPzJy41lJF4uLTPYSJNVr3qZjzQ656FCiwSCc9IpvbsSYsl7Oh
UgYfTj/pDSwwnNb+0FoAMd4v3rHsZa6sdw3bUDqHUHedXXctgQlT+mJUHrA5LWOl
G25sfImdC7S0EKiWq7XkbhuitlCH4tZsYACIUbHAtjKLpjh5tmF4CNnEPx1dUxQs
VnOB921gd1Cu1n5jHU4GzjcLmQAQCvev7wIDAQABo4IDyDCCA8QwDgYDVR0PAQH/
BAQDAgWgMIGJBggrBgEFBQcBAQR9MHswQgYIKwYBBQUHMAKGNmh0dHA6Ly9zZWN1
cmUyLmFscGhhc3NsLmNvbS9jYWNlcnQvZ3NhbHBoYXNoYTJnMnIxLmNydDA1Bggr
BgEFBQcwAYYpaHR0cDovL29jc3AyLmdsb2JhbHNpZ24uY29tL2dzYWxwaGFzaGEy
ZzIwVwYDVR0gBFAwTjBCBgorBgEEAaAyAQoKMDQwMgYIKwYBBQUHAgEWJmh0dHBz
Oi8vd3d3Lmdsb2JhbHNpZ24uY29tL3JlcG9zaXRvcnkvMAgGBmeBDAECATAJBgNV
HRMEAjAAMD4GA1UdHwQ3MDUwM6AxoC+GLWh0dHA6Ly9jcmwyLmFscGhhc3NsLmNv
bS9ncy9nc2FscGhhc2hhMmcyLmNybDArBgNVHREEJDAighAqLmNsb3VkY2FtLnNw
YWNlgg5jbG91ZGNhbS5zcGFjZTAdBgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUH
AwIwHQYDVR0OBBYEFBCC9S/Ti9HajS8ws8gfOxgixJYfMB8GA1UdIwQYMBaAFPXN
1TwIUPlqTzq3l9pWg+Zp0mj3MIIB9AYKKwYBBAHWeQIEAgSCAeQEggHgAd4AdwC7
2d+8H4pxtZOUI5eqkntHOFeVCqtS6BqQlmQ2jh7RhQAAAV57NW2bAAAEAwBIMEYC
IQDiYv6g4vJf6pUA+vhKGnVPMBWhqfgw+fMJuh5sLWqB1wIhAP6btIpWFeB+s+Gu
w9WDoFgZkts4Nn72NkTcbdlBA6SeAHUAVhQGmi/XwuzT9eG9RLI+x0Z2ubyZEVzA
75SYVdaJ0N0AAAFeezVt3wAABAMARjBEAiADi2JrW0zfcZEUjRQzEq5LWAU0YOPa
ZfMS7UCUUIp5tAIgNB0989nOxNHCgUvOV+iKQE6t1vOrfzxd+xK06erfEuMAdQCk
uQmQtBhYFIe7E6LMZ3AKPDWYBPkb37jjd80OyA3cEAAAAV57NW18AAAEAwBGMEQC
IBHUTcBV8RkRkc4/qJhd1nD8M7RNk6X+VyYrHDLkeQWtAiBINuW00OTPqQcRzdjQ
0VY4P9EOPr79+JvHZ/TQzyoEpQB1AO5Lvbd1zmC64UJpH6vhnmajD35fsHLYgwDE
e4l6qP3LAAABXns1cGMAAAQDAEYwRAIgVSE8Ux1Fk6thVZFjK1enGSIRoGpIjC3g
SXD5zVMc3X4CIDeMkDafqTFLOB6fguxeJenfQsZw7kMJ43ZI5e3wGv4AMA0GCSqG
SIb3DQEBCwUAA4IBAQCwLLAyLej5Larjx2Ugv0erUyFMjsuHgmcHiEiPmy3TC1KU
ht4chUUNHecrS6/jXvudkvMHQyaWzjngKNO8KkBpHJBfhDlX2m+j3CoXJ+6x/t2l
7TrcMlaufQfYOcPE1ax7xAa/8aQiYzkxAHT1osg1VjvGv/apTsezwxG8rjdzDfaz
QQK0yellrvqPchjmTqE5GvfbotTdVAr6VgnLVzjnf1y0EMIYFhrBgJ0S26Iomlvx
+u13zrhp9mCqBypBse8BLsQZfFG6JU3M/FyzcmEOpeJ6+54GmmTgx6NsyzNc3oxJ
qWsFLW3Ne89nnlgJUaatymaahs2nQ69ryqoKvXLf
-----END CERTIFICATE-----
"""

# Janus SSL cert private key encrypted with the corresponding KMS key
janus_kms_key_alias = "alias/janus"
janus_encrypted_ssl_key_pem = """
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
"""

lightsail = boto3.client('lightsail')
route53 = boto3.client('route53')
cloudwatch_us_east_1 = boto3.client('cloudwatch',
                                    region_name='us-east-1')  # this is required for Route53 health check alarms
kms = boto3.client('kms')
sts = boto3.client('sts')

aws_account_id = sts.get_caller_identity()['Account']

# Route53 health check alarms topic must reside in us-east-1 so it's created elsewhere and referenced here by the arn
janus_health_check_alarms_topic_arn = f'arn:aws:sns:us-east-1:{aws_account_id}:JanusHealthCheckAlarms'


def get_lightsail_init_script():
    # the more logical place to decrypt the SSL key would be the lightsail instance itself, however they somehow
    # exist under a different aws account id, so kms key policy would need to be modified to allow that
    janus_ssl_key_pem = kms.decrypt(CiphertextBlob=base64.b64decode(janus_encrypted_ssl_key_pem))['Plaintext'].decode(
        "ascii")
    return """
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
docker run -d --name docker-janus --restart=always -p 8080:8080 -p 8088:8088 -p 8089:8089 \
    -p 7889:7889 -p 8188:8188 -p 10000-10200:10000-10200/udp -p 20000-21000:20000-21000/udp \
    -e SSL_CERT_PEM="$(cat <<'EOF'
{0}
EOF
)" -e SSL_KEY_PEM="$(cat <<'EOF'
{1}
EOF
)" {2}
""".format(janus_ssl_cert_pem, janus_ssl_key_pem, lightsail_janus_image)


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
    logger.info(f'Creating instance: {instance_name}, {lightsail_az}, {lightsail_blueprint_id}, {lightsail_bundle_id}')
    # create/start the instance (Janus docker container will be set up/started via lightsail_init_script)
    lightsail.create_instances(instanceNames=[instance_name],
                               availabilityZone=lightsail_az,
                               blueprintId=lightsail_blueprint_id,
                               bundleId=lightsail_bundle_id,
                               userData=get_lightsail_init_script())
    # wait until the instance is running
    for _ in range(120):
        time.sleep(1)
        if get_instance_status(instance_name) == 'running':
            break
    logger.info(f'Instance {instance_name} is running')
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
            'Changes': [{
                'Action': 'UPSERT',
                'ResourceRecordSet': {
                    'Name': domain_name,
                    'Type': 'A',
                    'TTL': 600,
                    'ResourceRecords': [{
                        'Value': ipv4_address
                    }]
                }
            }]
        })
    logger.info(f'Created {domain_name} DNS record for the {instance_name} instance')
    # create route53 health check for the REST API (https on port 8089)
    healthcheck_id = route53.create_health_check(
        CallerReference=domain_name,
        HealthCheckConfig={
            'Port': 8089,
            'Type': 'HTTPS',
            'ResourcePath': '/janus/info',
            'FullyQualifiedDomainName': domain_name,
            'RequestInterval': 30,
            'FailureThreshold': 10,  # 30*10 = 5 minutes
            'MeasureLatency': False
        }
    )['HealthCheck']['Id']
    route53.change_tags_for_resource(
        ResourceType='healthcheck',
        ResourceId=healthcheck_id,
        AddTags=[{
            'Key': 'Name',
            'Value': f'Health check on https://{domain_name}:8089/janus/info endpoint'
        }]
    )
    # the health check alarm must reside in us-east-1 since Route53 can only work with such
    cloudwatch_us_east_1.put_metric_alarm(
        AlarmName=f'{domain_name}',
        AlarmDescription=f'Alarm for Route53 health check on https://{domain_name}:8089/janus/info endpoint',
        ActionsEnabled=True,
        AlarmActions=[janus_health_check_alarms_topic_arn],
        MetricName='HealthCheckStatus',
        Namespace='AWS/Route53',
        Statistic='Minimum',
        Dimensions=[{
            'Name': 'HealthCheckId',
            'Value': healthcheck_id
        }],
        Period=60,
        EvaluationPeriods=1,
        Threshold=1.0,
        ComparisonOperator='LessThanThreshold'
    )
    logger.info(f'Created health check with id {healthcheck_id}')
    return {'name': instance_name}


def remove_health_checks(domain_name):
    """Removes Route53 health checks matching supplied domain name"""
    paginator = route53.get_paginator('list_health_checks')
    response_iterator = paginator.paginate()
    for page in response_iterator:
        for health_check in page['HealthChecks']:
            health_check_id = health_check['Id']
            health_check_config = health_check['HealthCheckConfig']
            if health_check_config['FullyQualifiedDomainName'] == domain_name:
                logger.info(f'Removing health check {health_check_id} and associated alarms')
                route53.delete_health_check(
                    HealthCheckId=health_check_id
                )
                cloudwatch_us_east_1.delete_alarms(
                    AlarmNames=[f'{domain_name}']
                )


def remove_janus_instance(instance_name):
    """Removes a Lightsail instance and associated DNS records and health checks"""
    logger.info(f'Removing Janus instance {instance_name}')
    # get instance ip addresses
    domain_name = f'{instance_name}.{janus_hosted_zone_domain}'
    # query route53 for the ip address since we need it to remove the dns record/health checks
    ipv4_address = route53.test_dns_answer(
        HostedZoneId=janus_hosted_zone_id,
        RecordName=domain_name,
        RecordType='A'
    )['RecordData'][0]
    # delete the health check
    remove_health_checks(domain_name)
    # remove the dns alias
    route53.change_resource_record_sets(
        HostedZoneId=janus_hosted_zone_id,
        ChangeBatch={
            'Changes': [{
                'Action': 'DELETE',
                'ResourceRecordSet': {
                    'Name': domain_name,
                    'Type': 'A',
                    'TTL': 600,
                    'ResourceRecords': [{
                        'Value': ipv4_address
                    }]
                }
            }]
        })
    # delete the instance
    lightsail.delete_instance(instanceName=instance_name)


def handler(event, context):
    """Ensures the specified number of Janus container instances are running"""
    logger.info(json.dumps(event, sort_keys=True, indent=4))

    # lambda parameters
    required_instance_count = event.get('requiredInstanceCount')
    alarms = list(filter(None, map(lambda record: json.loads(record.get('Sns', {}).get('Message', '')),
                                   event.get('Records', []))))

    instances = get_janus_instances()
    current_instance_count = len(instances)

    # required_instance_count is specified, add/remove instances until the actual count matches the required_instance_count
    if required_instance_count is not None:
        logger.info(
            f'Janus instance required count: {required_instance_count}, current count: {current_instance_count}')
        if current_instance_count > required_instance_count:
            for _ in range(current_instance_count - required_instance_count):
                remove_janus_instance(instances[-1]['name'])
                instances.pop()
        elif current_instance_count < required_instance_count:
            for _ in range(required_instance_count - current_instance_count):
                create_janus_instance()

    # if we are called via health check alarm actions, replace dead instances with new ones
    if alarms:
        logger.info(f'Alarms: {alarms}')
        for alarm in alarms:
            domain_name = alarm['AlarmName']
            matching_instances = list(filter(lambda instance: instance['name'] in domain_name, instances))
            for matching_instance in matching_instances:
                instance_name = matching_instance['name']
                logger.info(f'Instance {instance_name} is dead, replacing it with a new one')
                remove_janus_instance(instance_name)
                create_janus_instance()
