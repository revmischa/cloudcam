import boto3
# import botocore
import unittest
import logging
import time
import os
import requests
import tempfile
import ssl
from functools import partial
from AWSIoTPythonSDK.MQTTLib import AWSIoTMQTTClient
from AWSIoTPythonSDK.exception.AWSIoTExceptions import connectTimeoutException


boto3.set_stream_logger('botocore', logging.WARN)
log = logging.getLogger('cloudcam.test')
log.setLevel(logging.DEBUG)

iotcore = logging.getLogger("AWSIoTPythonSDK.core")
iotcore.setLevel(logging.DEBUG)


class Client():
    def __init__(self, iot_client=None, iot_data_client=None, credentials=None, ca_path=None, privkey_path=None, cert_path=None):
        assert ca_path, "Certificate is required"
        if not iot_client:
            iot_client = boto3.client('iot')
        if not iot_data_client:
            iot_data_client = boto3.client('iot-data')
        self.iot_client = iot_client
        self.iot_data_client = iot_data_client
        self.cert_path = cert_path
        self.privkey_path = privkey_path
        self.ca_path = ca_path
        self.credentials = credentials
        self.init_mqtt_client()

    def init_mqtt_client(self):
        endpoint = self.iot_client.describe_endpoint()
        use_websocket = True if self.credentials else False
        endpoint_port = 443 if use_websocket else 8883
        self.mqtt_client = AWSIoTMQTTClient("testo", useWebsocket=use_websocket)
        self.mqtt_client.configureEndpoint(endpoint['endpointAddress'], endpoint_port)
        self.mqtt_client.configureOfflinePublishQueueing(-1)
        self.mqtt_client.configureConnectDisconnectTimeout(10)
        self.mqtt_client.configureMQTTOperationTimeout(10)
        self.configure_credentials()
        log.debug("OpenSSL version {}".format(ssl.OPENSSL_VERSION))
        log.debug("Connecting MQTT client to {} on port {}...".format(endpoint['endpointAddress'], endpoint_port))
        try:
            self.mqtt_client.connect()
            log.debug("MQTT client connected")
        except connectTimeoutException:
            log.error("Failed to connect MQTT client - timeout (check policy)")
            self.mqtt_client = None

    def configure_credentials(self):
        if self.credentials:
            self.mqtt_client.configureIAMCredentials(*(self.credentials.values()))
            self.mqtt_client.configureCredentials(self.ca_path)
        elif self.privkey_path and self.cert_path:
            log.debug("Using %s %s %s", str(self.ca_path), str(self.privkey_path), str(self.cert_path))
            self.mqtt_client.configureCredentials(self.ca_path, self.privkey_path, self.cert_path)
        else:
            raise Exception("No credentials found")


class Source(Client):
    def __init__(self, thing_name, **kw):
        self.thing_name = thing_name
        Client.__init__(self, **kw)
        if self.mqtt_client:
            self.mqtt_client.subscribe("cloudcam/thumb/request_snapshot", 1, partial(self.snapshot_requested_handler))
        else:
            log.warn("not subscribing - no mqtt_client")

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
        # get our acctid
        sts = boto3.client('sts')
        self.acct_id = sts.get_caller_identity().get('Account')
        self.region_name = boto3.session.Session().region_name
        # generate a principal CA cert to connect our thing to
        self.master_iot_client = boto3.client('iot')
        self.create_test_certificate()
        self.create_test_policy()
        self.thing_name = self.create_test_thing()
        assume_creds = self.assume_source_role()
        self.iot_client = boto3.client('iot', **assume_creds)
        self.iot_data_client = boto3.client('iot-data', **assume_creds)
        self.source = Source(
            self.thing_name,
            ca_path=self.ca_path,
            privkey_path=self.privkey_path,
            cert_path=self.cert_path,
            iot_client=self.iot_client,
            iot_data_client=self.iot_data_client,
        )
        if not self.source.mqtt_client:
            # failed to connect MQTT, bail
            self.tearDown()
            self.fail("Could not connect to MQTT server")
            # self.skipTest("Failed to connect MQTT client")

    def tearDown(self):
        print("TEARDOWN")
        log.info("cleaning up stuff in tearDown")
        if self.source.mqtt_client:
            self.source.mqtt_client.disconnect()
        self.delete_test_policy()
        self.delete_test_thing()
        self.delete_test_certificate()

    #####

    def create_test_certificate(self):
        # generate keypair
        self.cert = self.master_iot_client.create_keys_and_certificate(setAsActive=True)
        self.iot_principal = self.cert['certificateArn']
        cert = self.cert["certificatePem"]
        privkey = self.cert['keyPair']['PrivateKey']
        self.cert_path = self.write_temp(cert)
        self.privkey_path = self.write_temp(privkey)
        # get verisign root CA for MQTT TLS connection
        r = requests.get("https://www.symantec.com/content/en/us/enterprise/verisign/roots/VeriSign-Class%203-Public-Primary-Certification-Authority-G5.pem")
        verisign = r.content
        self.ca_path = self.write_temp(verisign)
        # print("cert", cert, "privkey", privkey, "ca", verisign)

    def delete_test_policy(self, policy_name="test-policy"):
        iot = self.master_iot_client
        # check if already exists
        policies = iot.list_policies()['policies']
        policy_names = [p['policyName'] for p in policies]
        if policy_name not in policy_names:
            return
        policy = iot.get_policy(policyName=policy_name)
        if policy:
            principals = iot.list_policy_principals(policyName=policy_name)['principals']
            # detach first
            for principal in principals:
                iot.detach_principal_policy(
                    policyName=policy_name,
                    principal=principal,
                )
            iot.delete_policy(policyName=policy_name)

    def create_test_policy(self):
        policy_name = "test-policy"
        iot = self.master_iot_client
        self.delete_test_policy(policy_name)
        # need a policy that allows connect otherwise connect() will timeout forever
        policy_document = """{{
          "Version": "2012-10-17",
          "Statement": [
            {{
                "Action": ["iot:Connect"],
                "Resource": ["*"],
                "Effect": "Allow"
            }},
            {{
                "Effect": "Allow",
                "Action": ["iot:*"],
                "Resource": [
                    "arn:aws:iot:{region}:{aid}:topicfilter/$aws/things/CameraTest/*",
                    "arn:aws:iot:{region}:{aid}:topicfilter/cloudcam/thumb/request_snapshot",
                    "arn:aws:iot:{region}:{aid}:topic/cloudcam/thumb/store"
                ]
            }}
          ]
        }}""".format(aid=self.acct_id, region=self.region_name)
        iot.create_policy(
            policyName=policy_name,
            policyDocument=policy_document,
        )
        iot.attach_principal_policy(
            principal=self.iot_principal,
            policyName=policy_name,
        )

    def write_temp(self, content):
        # write it out
        f = tempfile.NamedTemporaryFile(delete=False)
        f.write(content)
        f.flush()
        return f.name

    def delete_test_certificate(self):
        self.master_iot_client.update_certificate(certificateId=self.cert['certificateId'], newStatus='INACTIVE')
        self.master_iot_client.delete_certificate(certificateId=self.cert['certificateId'])
        # delete CA file
        os.unlink(self.ca_path)
        os.unlink(self.cert_path)
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
