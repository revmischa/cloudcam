import boto3
import logging

log = logging.getLogger("cloudcam")
log.setLevel(logging.DEBUG)

iot = boto3.client("iot")


def handler(event, context):
    """Automatically confirms newly created Cognito users"""
    log.debug(f"event: {event}")
    event['response'] = {
        # set this so all newly registered users are automatically confirmed/enabled (this defaults to false)
        'autoConfirmUser': True,
        'autoVerifyEmail': True  # don't ask user for email confirmation code
    }

    return event
