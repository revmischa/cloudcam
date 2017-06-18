import boto3
import logging

log = logging.getLogger("cloudcam")
log.setLevel(logging.DEBUG)

iot = boto3.client("iot")


def handler(event, context):
    log.debug(f"event: {event} context: {context}")
    event['response'] = {
        # set this so all newly registered users are automatically confirmed/enabled (this defaults to false)
        'autoConfirmUser': True
    }
    return event
