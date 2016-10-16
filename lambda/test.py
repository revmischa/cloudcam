import boto3
# import botocore
import unittest
import logging
import time
import os
import requests
import tempfile
from functools import partial
from AWSIoTPythonSDK.MQTTLib import AWSIoTMQTTClient


boto3.set_stream_logger('botocore', logging.WARN)
log = logging.getLogger('cloudcam.test')


class Client():
    def __init__(self, iot_client=None, iot_data_client=None, credentials=None, cert_path=None):
        assert cert_path, "Certificate is required"
        if not iot_client:
            iot_client = boto3.client('iot')
        if not iot_data_client:
            iot_data_client = boto3.client('iot-data')
        self.cert_path = cert_path
        self.iot_client = iot_client
        self.iot_data_client = iot_data_client
        self.init_mqtt_client()
        if credentials:
            self.mqtt_client.configureIAMCredentials(*(credentials.values()))

    def init_mqtt_client(self):
        endpoint = self.iot_client.describe_endpoint()
        self.mqtt_client = AWSIoTMQTTClient("test", useWebsocket=True)
        self.mqtt_client.configureEndpoint(endpoint['endpointAddress'], 443)
        self.mqtt_client.configureCredentials(self.cert_path)
        self.mqtt_client.connect()


class Source(Client):
    def __init__(self, thing_name, **kw):
        self.thing_name = thing_name
        Client.__init__(self, **kw)
        self.mqtt_client.subscribe("cloudcam/thumb/request_snapshot", 1, partial(self.snapshot_requested_handler))

    def req_thumb_update(self, cb):
        if cb:
            setattr(self, "_thumb_cb", cb)
        self.iot_data_client.publish(
            topic="cloudcam/thumb/store",
        )

    def snapshot_requested_handler(self, client, userdata, message):
        log.debug("received snapshot request:", message)
        cb = getattr(self, "_thumb_cb")
        if cb:
            cb(client, userdata, message)
            delattr(self, "_thumb_cb")


class TestCloudcamSource(unittest.TestCase):
    def setUp(self):
        # generate a principal CA cert to connect our thing to
        self.master_iot_client = boto3.client('iot')
        self.create_test_certificate()
        self.iot_principal = self.ca['certificateArn']
        self.thing_name = self.create_test_thing()
        assume_creds = self.assume_source_role()
        self.iot_client = boto3.client('iot', **assume_creds)
        self.iot_data_client = boto3.client('iot-data', **assume_creds)
        self.source = Source(
            self.thing_name,
            cert_path=self.root_ca_path,
            iot_client=self.iot_client,
            iot_data_client=self.iot_data_client,
            credentials=assume_creds,
        )

    def tearDown(self):
        self.delete_test_thing()
        self.delete_test_certificate()

    #####

    def create_test_certificate(self):
        self.ca = self.master_iot_client.create_keys_and_certificate(setAsActive=True)
        self.pubkey_path = self.write_temp(self.ca['keyPair']['PublicKey'])
        self.privkey_path = self.write_temp(self.ca['keyPair']['PrivateKey'])
        # get verisign root CA for MQTT TLS connection
        r = requests.get("https://www.symantec.com/content/en/us/enterprise/verisign/roots/VeriSign-Class%203-Public-Primary-Certification-Authority-G5.pem")
        verisign = r.content
        self.root_ca_path = self.write_temp(verisign)

    def write_temp(self, content):
        # write it out
        f = tempfile.NamedTemporaryFile(delete=False)
        f.write(content)
        f.close()
        return f.name

    def delete_test_certificate(self):
        self.master_iot_client.update_certificate(certificateId=self.ca['certificateId'], newStatus='INACTIVE')
        self.master_iot_client.delete_certificate(certificateId=self.ca['certificateId'])
        # delete CA file
        os.unlink(self.root_ca_path)
        os.unlink(self.pubkey_path)
        os.unlink(self.privkey_path)

    def create_test_thing(self):
        res = self.master_iot_client.create_thing(
            thingName='cloudcam-test-thing',
            thingTypeName='cloudcam-source',
            attributePayload={},
        )
        thing_name = res['thingName']
        self.master_iot_client.attach_thing_principal(
            thingName=thing_name,
            principal=self.iot_principal,
        )
        return thing_name

    def assume_source_role(self):
        # drop privs to cloudcam_source
        sts = boto3.client('sts')
        self.acct_id = sts.get_caller_identity().get('Account')
        assume_res = sts.assume_role(
            RoleArn="arn:aws:iam::{}:role/cloudcam_source".format(self.acct_id),
            RoleSessionName="CloudcamTestSource",
        )
        credentials = assume_res['Credentials']
        assume_creds = dict(
            aws_access_key_id=credentials['AccessKeyId'],
            aws_secret_access_key=credentials['SecretAccessKey'],
            aws_session_token=credentials['SessionToken'],
        )
        return assume_creds

    def delete_test_thing(self):
        principals = self.master_iot_client.list_thing_principals(thingName=self.thing_name)['principals']
        # detach from everything (may be leftover attachments to this thing apparently...)
        for principal in principals:
            log.debug("Detaching thing {} from principal {}".format(self.thing_name, principal))
            self.master_iot_client.detach_thing_principal(
                thingName=self.thing_name,
                principal=principal,
            )
        self.master_iot_client.delete_thing(thingName=self.thing_name)

    #####

    def test_iot_thumb(self):
        self.iot_client.update_thing(
            thingName=self.thing_name,
            attributePayload={},
        )

        # request info to update thumbnail
        self.thumb_update_res = None

        def thumb_updated(client, userdata, message):
            log.debug("GOT thumb update", message)
            self.thumb_update_res = message
        self.source.req_thumb_update(thumb_updated)

        # wait until we receive a response to our request
        start = time.time()
        while not self.thumb_update_res and time.time() < start + 4:
            time.sleep(0.5)

        if not self.thumb_update_res:
            self.fail("Failed to get MQTT response message for req_thumb_update()")
            return
        print(self.thumb_update_res)
