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
    """Requests an updated thumbnail from the thing via the iot thing shadow"""
    # lambda parameters
    thing_names = event['thingNames']
    if not thing_names:
        raise Exception("thingNames must be specified")

    # update iot things shadows with presignad thumbnail upload/download URLs
    for thing_name in thing_names:
        # note: security is handled by AWS IoT policies attached to the
        # Cognito identity, so we don't perform any checks here
        iot_data.publish(
            topic=f'cloudcam/{thing_name}/commands',
            qos=1,
            payload=json.dumps({'command': 'upload_thumb',
                                'upload_url': gen_upload_url(thing_name),
                                'download_url': gen_download_url(thing_name)
                                }).encode('utf-8'))
    return {}


def gen_upload_url(thing_name):
    """Returns a presigned upload (PUT) URL for the specified thing thumbnail"""
    return s3.generate_presigned_url('put_object',
                                     Params={'Bucket': thumb_bucket_name, 'Key': f'thumb/{thing_name}.jpg'},
                                     ExpiresIn=3600, HttpMethod='PUT')


def gen_download_url(thing_name):
    """Returns a presigned download (GET) URL for the specified thing thumbnail"""
    return s3.generate_presigned_url('get_object',
                                     Params={'Bucket': thumb_bucket_name, 'Key': f'thumb/{thing_name}.jpg'},
                                     ExpiresIn=3600, HttpMethod='GET')
