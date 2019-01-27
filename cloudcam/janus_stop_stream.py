import json
import logging

import boto3

logger = logging.getLogger()
logger.setLevel(logging.INFO)

iot = boto3.client('iot')
iot_data = boto3.client('iot-data')


def handler(event, context):
    """Stop a RTP stream from the thing"""
    logger.info(json.dumps(event, sort_keys=True, indent=4))

    # lambda parameters
    thing_name = event['thingName']
    logger.info(thing_name)

    # retrieve iot thing shadow document
    thing_shadow = json.loads(iot_data.get_thing_shadow(thingName=thing_name)['payload'].read().decode('utf-8'))
    logger.info(json.dumps(thing_shadow, indent=2))

    # retrieve currently allocated stream info
    streams = thing_shadow['state']['desired'].get('streams')
    logger.info(f'Currently allocated streams: {streams}')

    # todo: client refcounting so camera could stop streaming if there are no clients
    # to do that we need a globally consistent data store (iot shadows aren't (?))
    # or we leave session management to the thing itself:
    # send messages to it when adding new or removing dead sessions, have the thing maintain a consistent list and
    # start/stop streaming depending on the list size
    # streams['current'] = None

    # update iot thing shadow with new stream data
    iot_data.update_thing_shadow(
        thingName=thing_name,
        payload=json.dumps({'state': {
            'desired': {
                'streams': streams
            }
        }}).encode('utf-8'))
