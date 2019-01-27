import json
import logging
import boto3
from slugify import slugify
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
    logger.info(f'identityId: {identity_id}')

    if not identity_id:
        raise Exception("Cognito identity not present")

    policies = iot.list_principal_policies(
        principal=identity_id
    )['policies']

    thing_names = []
    for policy in policies:
        policy_name = policy['policyName']
        thing_name = policy_name.replace(slugify(identity_id) + '-', '')
        if thing_exists_p(thing_name):
            thing_names.append(thing_name)

    return {
        "identityId": identity_id,
        "thingNames": thing_names,
        "policies": policies
    }
