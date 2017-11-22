import json
import logging
import boto3
from cloudcam import tools

logger = logging.getLogger()
logger.setLevel(logging.INFO)

iot = boto3.client('iot')
sts = boto3.client('sts')

default_thing_type_name = "Camera"

# standard AWS IoT root CA certificate; included here only so it could be
# shipped to the device as part of the JSON config file
root_ca_pem = """-----BEGIN CERTIFICATE-----
MIIE0zCCA7ugAwIBAgIQGNrRniZ96LtKIVjNzGs7SjANBgkqhkiG9w0BAQUFADCB
yjELMAkGA1UEBhMCVVMxFzAVBgNVBAoTDlZlcmlTaWduLCBJbmMuMR8wHQYDVQQL
ExZWZXJpU2lnbiBUcnVzdCBOZXR3b3JrMTowOAYDVQQLEzEoYykgMjAwNiBWZXJp
U2lnbiwgSW5jLiAtIEZvciBhdXRob3JpemVkIHVzZSBvbmx5MUUwQwYDVQQDEzxW
ZXJpU2lnbiBDbGFzcyAzIFB1YmxpYyBQcmltYXJ5IENlcnRpZmljYXRpb24gQXV0
aG9yaXR5IC0gRzUwHhcNMDYxMTA4MDAwMDAwWhcNMzYwNzE2MjM1OTU5WjCByjEL
MAkGA1UEBhMCVVMxFzAVBgNVBAoTDlZlcmlTaWduLCBJbmMuMR8wHQYDVQQLExZW
ZXJpU2lnbiBUcnVzdCBOZXR3b3JrMTowOAYDVQQLEzEoYykgMjAwNiBWZXJpU2ln
biwgSW5jLiAtIEZvciBhdXRob3JpemVkIHVzZSBvbmx5MUUwQwYDVQQDEzxWZXJp
U2lnbiBDbGFzcyAzIFB1YmxpYyBQcmltYXJ5IENlcnRpZmljYXRpb24gQXV0aG9y
aXR5IC0gRzUwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCvJAgIKXo1
nmAMqudLO07cfLw8RRy7K+D+KQL5VwijZIUVJ/XxrcgxiV0i6CqqpkKzj/i5Vbex
t0uz/o9+B1fs70PbZmIVYc9gDaTY3vjgw2IIPVQT60nKWVSFJuUrjxuf6/WhkcIz
SdhDY2pSS9KP6HBRTdGJaXvHcPaz3BJ023tdS1bTlr8Vd6Gw9KIl8q8ckmcY5fQG
BO+QueQA5N06tRn/Arr0PO7gi+s3i+z016zy9vA9r911kTMZHRxAy3QkGSGT2RT+
rCpSx4/VBEnkjWNHiDxpg8v+R70rfk/Fla4OndTRQ8Bnc+MUCH7lP59zuDMKz10/
NIeWiu5T6CUVAgMBAAGjgbIwga8wDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8E
BAMCAQYwbQYIKwYBBQUHAQwEYTBfoV2gWzBZMFcwVRYJaW1hZ2UvZ2lmMCEwHzAH
BgUrDgMCGgQUj+XTGoasjY5rw8+AatRIGCx7GS4wJRYjaHR0cDovL2xvZ28udmVy
aXNpZ24uY29tL3ZzbG9nby5naWYwHQYDVR0OBBYEFH/TZafC3ey78DAJ80M5+gKv
MzEzMA0GCSqGSIb3DQEBBQUAA4IBAQCTJEowX2LP2BqYLz3q3JktvXf2pXkiOOzE
p6B4Eq1iDkVwZMXnl2YtmAl+X6/WzChl8gGqCBpH3vn5fJJaCGkgDdk+bW48DW7Y
5gaRQBi5+MHt39tBquCWIMnNZBU4gcmU7qKEKQsTb47bDN0lAtukixlE0kF6BWlK
WE9gyn6CagsCqiUXObXbf+eEZSqVir2G3l6BFoMtEMze/aiCKm0oHw0LxOXnGiYZ
4fQRbxC1lfznQgUy286dUV4otp6F01vvpX1FQHKOtw5rDgb7MzVIcbidJ4vEZV8N
hnacRHr2lVz2XTIIM6RUthg/aFzyQkqFOFSDX9HoLPKsEdao7WNq
-----END CERTIFICATE-----
"""


def handler(event, context):
    """Provisions a new thing under the specified Cognito identity"""
    iot_endpoint = iot.describe_endpoint()
    region = iot_endpoint['endpointAddress'].split('.')[2]
    account_id = sts.get_caller_identity()['Account']

    # cognito identity id is passed via lambda context
    identity_id = context.identity.cognito_identity_id

    # lambda parameters
    thing_name = event['thingName']
    thing_type_name = event.get('thingTypeName', default_thing_type_name)
    client_id = event.get('clientId', thing_name)  # by default clientId and thingName are the same

    logger.info(
        f'identityId: {identity_id} thingName: {thing_name} thingTypeName: {thing_type_name} clientId: {client_id}')

    if not identity_id:
        raise Exception("Cognito identity not present")

    if not thing_name:
        raise Exception("thingName must be specified")

    if not client_id:
        raise Exception("clientId or thingName must be specified")

    # create iot thing
    iot.create_thing(
        thingName=thing_name,
        thingTypeName=thing_type_name,
        attributePayload={
            'attributes': {
                f'access:{identity_id}': 'owner'
            }
        }
    )

    # provision iot keys/certs
    keys_and_cert = iot.create_keys_and_certificate(setAsActive=True)

    certificate_id = keys_and_cert['certificateId']
    certificate_arn = keys_and_cert['certificateArn']

    # create certificate policy -- makes sure that only the client using
    # specified client_id and thing_name could connect using this certificate
    certificate_policy_name = f"{certificate_id}-{thing_name}"
    certificate_policy = {
        "Version": "2012-10-17",
        "Statement": [{
            "Effect": "Allow",
            "Action": [
                "iot:Connect"
            ],
            "Resource": f"arn:aws:iot:{region}:{account_id}:client/{client_id}"
        }, {
            "Effect": "Allow",
            "Action": [
                "iot:Publish",
                "iot:Receive"
            ],
            "Resource": [f"arn:aws:iot:{region}:{account_id}:topic/$aws/things/{thing_name}/shadow/*",
                         f"arn:aws:iot:{region}:{account_id}:topic/cloudcam/{thing_name}/commands",
                         f"arn:aws:iot:{region}:{account_id}:topic/cloudcam/{thing_name}/notifications"]
        }, {
            "Effect": "Allow",
            "Action": [
                "iot:Subscribe",
                "iot:Receive"
            ],
            "Resource": [f"arn:aws:iot:{region}:{account_id}:topicfilter/$aws/things/{thing_name}/shadow/*",
                         f"arn:aws:iot:{region}:{account_id}:topicfilter/cloudcam/{thing_name}/commands",
                         f"arn:aws:iot:{region}:{account_id}:topicfilter/cloudcam/{thing_name}/notifications"]
        }]
    }

    tools.ignore_resource_already_exists(iot.create_policy, policyName=certificate_policy_name,
                                         policyDocument=json.dumps(certificate_policy))
    iot.attach_principal_policy(policyName=certificate_policy_name, principal=certificate_arn)

    # note: there's an AWS limit of 10 IoT policies per principal (Cognito identity in this case)
    # so we need to either move to storing thing owner/group information in the thing name (say, prefix)
    # or use IoT rules (the limit is 1000 rules per account) to copy messages between the thing-owned and
    # identity-owned topics (using, perhaps, IoT SQL functions like topic(x) and substitution templates)

    # Cognito identity policy -- allows caller Cognito identity to interact with the thing
    identity_policy_name = f"{identity_id.replace(':', '_')}-{thing_name}"
    identity_policy = {
        "Version": "2012-10-17",
        "Statement": [{
            "Effect": "Allow",
            "Action": ["iot:Connect"],
            "Resource": [
                f"arn:aws:iot:{region}:{account_id}:client/wss-client-*"
            ]
        }, {
            "Effect": "Allow",
            "Action": ["iot:GetThingShadow",
                       "iot:UpdateThingShadow"],
            "Resource": [
                f"arn:aws:iot:{region}:{account_id}:thing/{thing_name}"
            ]
        }, {
            "Effect": "Allow",
            "Action": ["iot:Subscribe"],
            "Resource": [
                f"arn:aws:iot:{region}:{account_id}:topicfilter/$aws/things/{thing_name}/shadow/update",
                f"arn:aws:iot:{region}:{account_id}:topicfilter/$aws/things/{thing_name}/shadow/update/accepted",
                f"arn:aws:iot:{region}:{account_id}:topicfilter/$aws/things/{thing_name}/shadow/get",
                f"arn:aws:iot:{region}:{account_id}:topicfilter/$aws/things/{thing_name}/shadow/get/accepted",
                f"arn:aws:iot:{region}:{account_id}:topicfilter/cloudcam/{thing_name}/notifications",
                f"arn:aws:iot:{region}:{account_id}:topicfilter/cloudcam/{thing_name}/commands"
            ]
        }, {
            "Effect": "Allow",
            "Action": ["iot:Publish",
                       "iot:Receive"],
            "Resource": [
                f"arn:aws:iot:{region}:{account_id}:topic/$aws/things/{thing_name}/shadow/update",
                f"arn:aws:iot:{region}:{account_id}:topic/$aws/things/{thing_name}/shadow/update/accepted",
                f"arn:aws:iot:{region}:{account_id}:topic/$aws/things/{thing_name}/shadow/get",
                f"arn:aws:iot:{region}:{account_id}:topic/$aws/things/{thing_name}/shadow/get/accepted",
                f"arn:aws:iot:{region}:{account_id}:topic/cloudcam/{thing_name}/notifications",
                f"arn:aws:iot:{region}:{account_id}:topic/cloudcam/{thing_name}/commands"
            ]
        }]
    }

    logger.info(f'policy_name: {identity_policy_name} policy: {identity_policy}')

    tools.ignore_all(iot.detach_principal_policy, policyName=identity_policy_name, principal=identity_id)
    tools.ignore_all(iot.delete_policy, policyName=identity_policy_name)

    tools.ignore_resource_already_exists(iot.create_policy, policyName=identity_policy_name,
                                         policyDocument=json.dumps(identity_policy))
    iot.attach_principal_policy(policyName=identity_policy_name, principal=identity_id)

    # create thing config (which will be encoded as a single json file
    # containing all the required config data for the C client/thing)
    thing_config = {
        "thingName": thing_name,
        "thingTypeName": thing_type_name,
        "clientId": thing_name,
        "endpoint": iot_endpoint['endpointAddress'],
        "caPem": root_ca_pem,
        "certificatePem": keys_and_cert['certificatePem'],
        "certificatePrivateKey": keys_and_cert['keyPair']['PrivateKey']
    }

    return {
        "identityId": identity_id,
        "thingName": thing_name,
        "thingTypeName": thing_type_name,
        "clientId": client_id,
        "certificateId": certificate_id,
        "certificatePem": keys_and_cert['certificatePem'],
        "certificatePublicKey": keys_and_cert['keyPair']['PublicKey'],
        "certificatePrivateKey": keys_and_cert['keyPair']['PrivateKey'],
        "thingConfig": thing_config
    }
