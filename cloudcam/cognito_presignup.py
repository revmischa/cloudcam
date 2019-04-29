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
    """Automatically confirms newly created Cognito users"""
    log.debug(f"event: {event}")
    event['response'] = {
        # set this so all newly registered users are automatically confirmed/enabled (this defaults to false)
        'autoConfirmUser': True,
        'autoVerifyEmail': True  # don't ask user for email confirmation code
    }

    # attach our cognito user IoT policy

    # here userID is _WRONG_


    # the proper form here is region:userId
    identity = f"{event['region']}:{event['userName']}"
    print(f"identity: {identity}")
    # iot.attach_policy(policyName=user_iot_policy_name, target=identity)
    # print("Attached IoT policy")

    return event
