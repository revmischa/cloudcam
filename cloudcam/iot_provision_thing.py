import json
import logging
import boto3
from cloudcam import tools
from slugify import slugify

logger = logging.getLogger()

iot = boto3.client('iot')
sts = boto3.client('sts')

default_thing_type_name = "Camera"
default_thing_type_name = ""

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
    # cognito identity id is passed via lambda context
    identity_id = None
    # prefer cognito
    if hasattr(context, 'identity'):
        identity_id = context.identity.cognito_identity_id

    # lambda parameters
    thing_type_name = event.get('thingTypeName', default_thing_type_name)
    client_id = event.get('clientId')
    thing_name = slugify(event['thingName'])
    if not thing_name:
        raise Exception("thingName must be specified")

    provisioner = ThingProvisioner(thing_name=thing_name, cognito_identity_id=identity_id,
                                   client_id=client_id, thing_type=thing_type_name)
    return provisioner.provision()


class ThingProvisioner:
    cognito_identity_id: str = None
    region: str = None
    account_id: str = None
    thing_type: str = None
    client_id: str = None
    iot_endpoint = None
    thing_name: str

    def __init__(self, thing_name: str, cognito_identity_id: str = None, client_id: str = None, thing_type: str = None):
        self.cognito_identity_id = cognito_identity_id
        self.client_id = client_id
        self.thing_name = thing_name
        self.thing_type = thing_type

        if not client_id:
            # by default clientId and thingName are the same
            client_id = thing_name

        # get env info
        self.iot_endpoint = iot.describe_endpoint()
        self.region = self.iot_endpoint['endpointAddress'].split('.')[2]
        identity = sts.get_caller_identity()
        self.account_id = identity['Account']
        # print("identity", identity)

    def provision(self):
        # logger.info(f'identityId: {cognito_identity_id} thingName: {thing_name} thingTypeName: {thing_type} clientId: {client_id}')

        # create iot thing
        identity_id = self.cognito_identity_id
        create_thing = dict(
            thingName=self.thing_name,
            attributePayload={
                'attributes': {
                    f'access:{identity_id if identity_id else ""}': 'owner'
                }
            }
        )
        if self.thing_type:
            create_thing['thingTypeName'] = self.thing_type

        # do create
        create_res = iot.create_thing(**create_thing)
        thing_arn = create_res['thingArn']
        self.thing_arn = thing_arn

        # provision iot keys/certs
        keys_and_cert = iot.create_keys_and_certificate(setAsActive=True)

        # allow thing to connect and do stuff
        self.attach_thing_policy(keys_and_cert=keys_and_cert)

        # allow cognito user to access thing
        if self.cognito_identity_id:
            self.attach_principal_policy()

        # create thing config (which will be encoded as a single json file
        # containing all the required config data for the C client/thing)
        thing_config = {
            "thingName": self.thing_name,
            "thingTypeName": self.thing_type,
            "clientId": self.thing_name,
            "endpoint": self.iot_endpoint['endpointAddress'],
            "caPem": root_ca_pem,
            "certificatePem": keys_and_cert['certificatePem'],
            "certificatePrivateKey": keys_and_cert['keyPair']['PrivateKey']
        }

        return {
            "identityId": self.cognito_identity_id,
            "thingName": self.thing_name,
            "thingTypeName": self.thing_type,
            "clientId": self.client_id,
            "certificatePem": keys_and_cert['certificatePem'],
            "certificatePublicKey": keys_and_cert['keyPair']['PublicKey'],
            "certificatePrivateKey": keys_and_cert['keyPair']['PrivateKey'],
            "thingConfig": thing_config
        }

    def attach_thing_policy(self, keys_and_cert):
        region = self.region
        account_id = self.account_id
        thing_name = self.thing_name
        certificate_id = keys_and_cert['certificateId']
        certificate_arn = keys_and_cert['certificateArn']

        # create certificate policy -- makes sure that only the client using
        # specified client_id and thing_name could connect using this certificate
        certificate_policy_name = f"{certificate_id}-{self.thing_name}"
        certificate_policy = {
            "Version": "2012-10-17",
            "Statement": [{
                "Effect": "Allow",
                "Action": [
                    "iot:Connect"
                ],
                "Resource": f"arn:aws:iot:{region}:{account_id}:client/{self.client_id}"
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

        # create policy
        tools.ignore_resource_already_exists(iot.create_policy, policyName=certificate_policy_name,
                                             policyDocument=json.dumps(certificate_policy))
        # attach policy to certificate
        iot.attach_principal_policy(policyName=certificate_policy_name, principal=certificate_arn)
        # connect cert to thing
        iot.attach_policy(policyName=certificate_policy_name, target=self.thing_arn)

    def attach_principal_policy(self):
        region = self.region
        account_id = self.account_id
        thing_name = self.thing_name
        # note: there's an AWS limit of 10 IoT policies per principal (Cognito identity in this case)
        # so we need to either move to storing thing owner/group information in the thing name (say, prefix)
        # or use IoT rules (the limit is 1000 rules per account) to copy messages between the thing-owned and
        # identity-owned topics (using, perhaps, IoT SQL functions like topic(x) and substitution templates)

        # Cognito identity policy -- allows caller Cognito identity to interact with the thing
        identity_policy_name = f"{slugify(self.cognito_identity_id)}-{thing_name}"
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
                "Action": ["iot:GetThingShadow", "iot:UpdateThingShadow"],
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
                "Action": ["iot:Publish", "iot:Receive"],
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

        tools.ignore_all(iot.detach_principal_policy, policyName=identity_policy_name, principal=self.cognito_identity_id)
        tools.ignore_all(iot.delete_policy, policyName=identity_policy_name)

        tools.ignore_resource_already_exists(iot.create_policy, policyName=identity_policy_name,
                                             policyDocument=json.dumps(identity_policy))
        iot.attach_principal_policy(policyName=identity_policy_name, principal=self.cognito_identity_id)
