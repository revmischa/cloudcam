import logging
import boto3
from botocore.exceptions import ClientError

logger = logging.getLogger()
logger.setLevel(logging.INFO)

iot = boto3.client('iot')


def thing_exists_p(thing_name):
    try:
        iot.describe_thing(thingName=thing_name)
        return True
    except ClientError as e:
        if e.response["Error"]["Code"] == "ResourceNotFoundException":
            return False
        else:
            raise


def handler(event, context):
    """List things the current Cognito identity has access to"""
    # cognito identity id is passed via lambda context
    identity_id = context.identity.cognito_identity_id
    if not identity_id:
        raise Exception("Cognito identity not present")

    logger.info(f'listing things, identityId: {identity_id}')

    # TODO: pagination
    thing_names = iot.list_principal_things(
        maxResults=100,
        principal=identity_id,
    )['things']

    return {
        "thingNames": thing_names,
    }
