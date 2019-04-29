import logging
import boto3
import os
from slugify import slugify
import requests
from cloudcam import tools
from typing import Dict, Optional, Tuple, Any
import json

logger = logging.getLogger()

iot = boto3.client('iot')
sts = boto3.client('sts')

default_thing_type_name = "Camera"
default_thing_type_name = ""

camera_iot_policy_name = os.getenv('CAMERA_IOT_POLICY_NAME')
if not camera_iot_policy_name:
    raise Exception("Missing CAMERA_IOT_POLICY_NAME")


def handler(event, context):
    """Provisions a new thing under the specified Cognito identity"""
    # cognito identity id is passed via lambda context
    identity_id = None
    # prefer cognito
    if hasattr(context, 'identity'):
        identity_id = context.identity.cognito_identity_id
        print(f"id: {identity_id}")

    # lambda parameters
    thing_type_name = event.get('thingTypeName', default_thing_type_name)
    client_id = event.get('clientId')
    thing_name = slugify(event['thingName'])
    if not thing_name:
        raise Exception("thingName must be specified")

    provisioner = ThingProvisioner(thing_name=thing_name,
                                   cognito_identity_id=identity_id,
                                   client_id=client_id,
                                   thing_type=thing_type_name)
    return provisioner.provision()


class ThingProvisioner:
    cognito_identity_id: Optional[str] = None
    region: Optional[str] = None
    account_id: Optional[str] = None
    thing_type: Optional[str] = None
    client_id: str = Optional[None]
    iot_endpoint = None
    thing_name: str
    root_ca_pem: Optional[str] = None

    def __init__(self,
                 thing_name: str,
                 cognito_identity_id: str = None,
                 client_id: str = None,
                 thing_type: str = None):
        self.cognito_identity_id = cognito_identity_id
        self.thing_name = thing_name
        self.thing_type = thing_type

        if not client_id:
            # by default clientId and thingName are the same
            client_id = thing_name
        self.client_id = client_id

        # get env info
        self.iot_endpoint = iot.describe_endpoint(
            endpointType='iot:Data-ATS'
        )  # "We recommend that all customers create an Amazon Trust Services (ATS) endpoint..."
        self.root_ca_pem = self.get_root_ca()  # Amazon Root CA 1
        self.region = self.iot_endpoint['endpointAddress'].split('.')[2]
        identity = sts.get_caller_identity()
        self.account_id = identity['Account']

    def get_root_ca(self) -> str:
        """Get root CA certificate."""
        ca = requests.get(
            'https://www.amazontrust.com/repository/AmazonRootCA1.pem').text
        return ca

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
            })
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

        # allow user to interact with the device
        if identity_id:
            self.attach_identity_policy()

        # create thing config (which will be encoded as a single json file
        # containing all the required config data for the C client/thing)
        thing_config = {
            "thingName": self.thing_name,
            "thingTypeName": self.thing_type,
            "clientId": self.thing_name,
            "endpoint": self.iot_endpoint['endpointAddress'],
            "caPem": self.root_ca_pem,
            "certificatePem": keys_and_cert['certificatePem'],
            "certificatePrivateKey": keys_and_cert['keyPair']['PrivateKey']
        }

        return {
            "thingName": self.thing_name,
            "thingTypeName": self.thing_type,
            "certificatePublicKey": keys_and_cert['keyPair']['PublicKey'],
            "thingConfig": thing_config
        }

    def attach_thing_policy(self, keys_and_cert):
        certificate_arn = keys_and_cert['certificateArn']

        # attach policy to certificate
        iot.attach_policy(policyName=camera_iot_policy_name,
                          target=certificate_arn)

        # connect certificate to thing
        iot.attach_thing_principal(principal=certificate_arn,
                                   thingName=self.thing_name)

    def attach_identity_policy(self):
        # connect thing to identity
        iot.attach_thing_principal(principal=self.cognito_identity_id,
                                   thingName=self.thing_name)

        # generate policy
        identity_policy, identity_policy_name = self.generate_identity_policy()

        # (re-)create policy
        tools.ignore_all(iot.detach_policy,
                         policyName=identity_policy_name,
                         target=self.cognito_identity_id)
        tools.ignore_all(iot.delete_policy, policyName=identity_policy_name)
        tools.ignore_resource_already_exists(
            iot.create_policy,
            policyName=identity_policy_name,
            policyDocument=json.dumps(identity_policy))

        # attach policy to user
        iot.attach_policy(policyName=identity_policy_name,
                          target=self.cognito_identity_id)

    def generate_identity_policy(self) -> Tuple[Dict[str, Any], str]:
        region = self.region
        account_id = self.account_id
        thing_name = self.thing_name

        # note: there's an AWS limit of 10 IoT policies per principal (Cognito identity in this case)
        # so we need to either move to storing thing owner/group information in the thing name (say, prefix)
        # or use IoT rules (the limit is 1000 rules per account) to copy messages between the thing-owned and
        # identity-owned topics (using, perhaps, IoT SQL functions like topic(x) and substitution templates)

        # Cognito identity policy -- allows caller Cognito identity to interact with the thing
        identity_policy_name = f"{thing_name}-{slugify(self.cognito_identity_id)}"
        identity_policy = {
            "Version":
            "2012-10-17",
            "Statement": [{
                "Effect":
                "Allow",
                "Action": ["iot:Connect"],
                "Resource":
                [f"arn:aws:iot:{region}:{account_id}:client/wss-*"]
            }, {
                "Effect":
                "Allow",
                "Action": ["iot:GetThingShadow", "iot:UpdateThingShadow"],
                "Resource":
                [f"arn:aws:iot:{region}:{account_id}:thing/{thing_name}"]
            }, {
                "Effect":
                "Allow",
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
                "Effect":
                "Allow",
                "Action": ["iot:Publish", "iot:Receive"],
                "Resource": [
                    f"arn:aws:iot:{region}:{account_id}:topic/$aws/things/{thing_name}/shadow/update",
                    f"arn:aws:iot:{region}:{account_id}:topic/$aws/things/{thing_name}/shadow/update/accepted",
                    f"arn:aws:iot:{region}:{account_id}:topic/$aws/things/{thing_name}/shadow/get",
                    f"arn:aws:iot:{region}:{account_id}:topic/$aws/things/{thing_name}/shadow/get/accepted",
                    f"arn:aws:iot:{region}:{account_id}:topic/cloudcam/{thing_name}/notifications",
                    f"arn:aws:iot:{region}:{account_id}:topic/cloudcam/{thing_name}/commands",
                    f"arn:aws:iot:{region}:{account_id}:topic/cloudcam/{thing_name}/webrtc/setup"
                ]
            }]
        }

        logger.info(
            f'policy_name: {identity_policy_name} policy: {identity_policy}')
        return identity_policy, identity_policy_name
