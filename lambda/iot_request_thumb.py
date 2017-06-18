from __future__ import print_function

import json
import boto3
import logging
import os

log = logging.getLogger("cloudcam")
log.setLevel(logging.DEBUG)

thumb_bucket_name = os.getenv("S3_THUMB_BUCKET_NAME")

iot_data = boto3.client('iot-data')
s3 = boto3.client('s3')


def handler(event, context):
    log.info(json.dumps(event, indent=2))
    thing_names = event['thingNames']
    if not thing_names:
        raise Exception("thingNames must be specified")
    for thing_name in thing_names:
        # note: security is handled by AWS IoT policies attached to the
        # Cognito identity, so we don't perform any checks here
        iot_data.update_thing_shadow(
            thingName=thing_name,
            payload=json.dumps({'state': {
                'desired': {
                    'thumb_upload': {
                        'upload_url': gen_upload_url(thing_name),
                        'download_url': gen_download_url(thing_name)
                    }
                }
            }}).encode('utf-8'))
    return {}


def gen_upload_url(thing_name):
    return s3.generate_presigned_url('put_object',
                                     Params={'Bucket': thumb_bucket_name, 'Key': f'thumb/{thing_name}.jpg'},
                                     ExpiresIn=3600, HttpMethod='PUT')


def gen_download_url(thing_name):
    return s3.generate_presigned_url('get_object',
                                     Params={'Bucket': thumb_bucket_name, 'Key': f'thumb/{thing_name}.jpg'},
                                     ExpiresIn=3600, HttpMethod='GET')
