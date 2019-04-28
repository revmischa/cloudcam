import json
import logging
import boto3
from cloudcam import tools

logger = logging.getLogger()
logger.setLevel(logging.INFO)

iot = boto3.client('iot')
sts = boto3.client('sts')


def handler(event, context):
    logger.info(json.dumps(event, sort_keys=True, indent=4))

    iot_endpoint = iot.describe_endpoint()
    region = iot_endpoint['endpointAddress'].split('.')[2]
    account_id = sts.get_caller_identity()['Account']

    identity_id = context.identity.cognito_identity_id
    thing_name = event['thingName']

    logger.info(f'identityId: {identity_id} thingName: {thing_name}')

    if not identity_id:
        raise Exception("Cognito identity not present")

    if not thing_name:
        raise Exception("thingName must be specified")

    policy = {
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
    policy_name = f'{identity_id}-{thing_name}'.replace(':', '_')

    logger.info(f'policy_name: {policy_name} policy: {policy}')

    tools.ignore_all(iot.detach_principal_policy, policyName=policy_name, principal=identity_id)
    tools.ignore_all(iot.delete_policy, policyName=policy_name)

    tools.ignore_resource_already_exists(iot.create_policy, policyName=policy_name, policyDocument=json.dumps(policy))
    iot.attach_principal_policy(policyName=policy_name, principal=identity_id)

    iot.update_thing(
        thingName=thing_name,
        attributePayload={
            'attributes': {
                f'access:{identity_id}': 'owner'
            },
            'merge': True
        }
    )

    return {
        "thingName": thing_name,
        "identityId": identity_id,
        "policyName": policy_name
    }
