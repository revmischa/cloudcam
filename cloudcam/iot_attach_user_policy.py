"""After a user is created, we must grant them an IoT policy to allow them to connect to IoT MQTT.

Sadly, we can't do this without their cognito identity, which we apparently don't have in the Cognito triggers. That would be too easy."""

import boto3
import logging
import os

log = logging.getLogger("cloudcam")
log.setLevel(logging.DEBUG)
iot = boto3.client("iot")

user_iot_policy_name = os.getenv('USER_IOT_POLICY_NAME')
if not user_iot_policy_name:
    raise Exception("Missing USER_IOT_POLICY_NAME")


def handler(event, context):
    """Attach IoT policy to calling cognito user."""
    log.debug(f"event: {event} context: {context}")

    # attach our cognito user IoT policy
    if not hasattr(context, 'identity'):
        raise Exception("Attempting to attach IoT policy to user without Cognito identity in calling context")

    identity_id = context.identity.cognito_identity_id
    print(f"identity: {identity_id}")
    iot.attach_policy(policyName=user_iot_policy_name, target=identity_id)
    print("Attached IoT policy")
    return "ok"
