from __future__ import print_function

import json
import boto3
import logging
from pprint import pprint

log = logging.getLogger("cloudcam")
boto3.set_stream_logger('cloudcam', logging.DEBUG)


def save_thumb(event, context):
    log.debug("Received save_thumb event: " + json.dumps(event, indent=2))
    iot_client_id, thing_name = parse_event(event)
    if not iot_client_id:
        return
    presigned_upload_req = gen_upload_url()
    ret = {'upload_endpoint': presigned_upload_req}
    publish_upload_msg(ret)
    return {'RETURN': ret}  # i don't think this does anything


def parse_event(event):
    """Extract important information regarding the invocation of this lambda.

    :returns: iot_client_id, thing_name (subject to change)
    """
    test_thing_name = 'cloudcam-test-thing'
    iot_client_id = None
    thing_name = None
    iot = boto3.client('iot')
    if 'callerClientId' in event:
        # we should have a JSON message from our SQL query if this lambda was invoked
        # from an IoT rule
        iot_client_id = event['callerClientId']
        log.debug("got callerClientId", iot_client_id)
    if 'Records' in event:
        # invoked from SNS probably?
        event_records = event['Records']
        if not event_records:
            log.error("Failed to find event records in invocation event")
        sns_rec = event['Records'][0]['Sns']  # assume SNS for now
        if 'Message' not in sns_rec:
            log.error("Failed to find IoT message in invocation event")
        iot_message = json.loads(sns_rec['Message'])
        log.debug("SNS msg", iot_message)
        iot_client_id = iot_message['callerClientId']
    if 'callerPrincipal' in event:
        # this was invoked directly via a principal (IAM role probably)
        # at present this is only used for testing purposes and we
        # expect a test thing to exist to operate on.
        principal = event['callerPrincipal']
        things = iot.list_principal_things(principal=principal)['things']
        if test_thing_name not in things:
            log.error("Got invocation from principal", principal,
                      "but no test thing was found (or full, paginated list needs to be loaded maybe")
            pprint(event)
            return
        # ok our test thing exists for this principal so let's use that
        log.info("using", test_thing_name)
        thing_name = test_thing_name
    log.debug("iot_client_id", iot_client_id)
    if iot_client_id == 'n/a':
        iot_client_id = None
    if not thing_name and not iot_client_id:
        log.error("Failed to authenticate clientID. Event:", event)
    return iot_client_id, thing_name


def gen_upload_url():
    s3 = boto3.client('s3')
    return s3.generate_presigned_post("panop", "thumb/${filename}")


def publish_upload_msg(msg):
    iot = boto3.client('iot-data')
    msg_json = json.dumps(msg).encode('utf-8')
    print(msg_json)
    ret = iot.publish(
        topic='cloudcam/thumb/request_snapshot',
        qos=0,  # QoS 0 = at most once delivery
        payload=msg_json,
    )
    print("ret:", ret)
