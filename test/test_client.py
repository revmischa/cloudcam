import logging
import pytest
import json
import random
import string
import subprocess
import time
import threading
import os
from functools import partial
import paho.mqtt.client as mqtt
import cherrypy
from PIL import Image
import gi

gi.require_version('Gst', '1.0')
gi.require_version('GstRtspServer', '1.0')
from gi.repository import GObject, Gst, GstRtspServer

logger = logging.getLogger()
logger.setLevel(logging.INFO)

cloudcam_client_path = os.environ.get('CLOUDCAM_CLIENT', './client/c/cloudcam')


def rand_string(size=12, chars=string.ascii_uppercase + string.ascii_uppercase + string.digits):
    return ''.join(random.choice(chars) for _ in range(size))


class ClientTestParams:
    """Contains any parameters for running the tests"""
    # timeout for waits on any individual tests
    timeout = 20

    # host/port settings for various services
    http_host = '127.0.0.1'
    http_port = 18890

    mqtt_host = '127.0.0.1'
    mqtt_port = 8883

    rtsp_host = '127.0.0.1'
    rtsp_port = 8554

    rtp_host = '127.0.0.1'
    rtp_port = 47541

    # apparently there is a 19 character limit for thing names in the aws iot client implementation
    thing_name = 'test-thing-' + rand_string(8)

    ca_path = './certs/cloudcam-test-ca.pem'
    client_cert_path = './certs/cloudcam-client-test.crt'
    client_key_path = './certs/cloudcam-client-test.key'
    server_cert_path = './certs/cloudcam-server-test.crt'
    server_key_path = './certs/cloudcam-server-test.key'

    # mqtt topic names relevant to this test
    commands_topics = 'cloudcam/%s/commands' % thing_name
    shadow_update_topic = '$aws/things/%s/shadow/update' % thing_name
    shadow_delta_topic = '$aws/things/%s/shadow/update/delta' % thing_name


@pytest.fixture(scope="session")
def test_params():
    return ClientTestParams()


class ShadowState:
    """Stores IoT Device Shadow state"""
    # current iot shadow data and version number
    shadow_data = {}
    shadow_version = 1

    # a dict of functions to be called when a specific key in shadow_data is updated
    # { 'key': function, ... }
    shadow_handlers = {}


@pytest.fixture(scope="session")
def shadow_state():
    return ShadowState()


@pytest.fixture(scope="session")
def mqtt_broker(test_params):
    print("launching mqtt broker..")
    with open('mosquitto.conf', 'w') as f:
        f.write(
            "# mosquitto mqtt broker config for local client testing\n" +
            "persistence false\n"
            "connection_messages true\n"
            "allow_anonymous true\n"
            "require_certificate true\n"
            "log_type all\n"
            "bind_address " + test_params.mqtt_host + "\n" +
            "port " + str(test_params.mqtt_port) + "\n" +
            "cafile " + test_params.ca_path + "\n" +
            "certfile " + test_params.server_cert_path + "\n" +
            "keyfile " + test_params.server_key_path + "\n")
    broker_proc = subprocess.Popen(['mosquitto', '-v', '-c', 'mosquitto.conf'])
    yield broker_proc
    print("stopping mqtt broker..")
    broker_proc.terminate()


def on_connect(test_params, connection_event, mqtt_client, userdata, flags, rc):
    """Called by Paho MQTT client when it connects to the broker"""
    print("connected to mqtt broker with result code " + str(rc))
    mqtt_client.subscribe(test_params.commands_topics)
    mqtt_client.subscribe(test_params.shadow_delta_topic)
    mqtt_client.subscribe(test_params.shadow_update_topic)
    connection_event.set()


def on_message(test_params, shadow_state, mqtt_client, userdata, msg):
    """Called by Paho MQTT client when a message arrives"""
    print(msg.topic + " " + str(msg.payload))
    if msg.topic == test_params.shadow_update_topic:
        data = json.loads(msg.payload.decode('utf-8'))
        assert isinstance(data, dict)
        reported = data['state']['reported']
        assert isinstance(reported, dict)
        shadow_state.shadow_data = {**shadow_state.shadow_data, **reported}
        print("iot shadow update; new state: " + json.dumps(shadow_state.shadow_data, indent=2))
        for k, v in reported.items():
            if shadow_state.shadow_handlers.get(k):
                shadow_state.shadow_handlers[k](v)


@pytest.fixture(scope="session")
def mqtt_client(test_params, mqtt_broker, shadow_state):
    print("starting mqtt client..")
    connection_event = threading.Event()
    mqtt_client = mqtt.Client(client_id='test-driver')
    mqtt_client.on_connect = partial(on_connect, test_params, connection_event)
    mqtt_client.on_message = partial(on_message, test_params, shadow_state)
    mqtt_client.tls_set(ca_certs=test_params.ca_path, certfile=test_params.client_cert_path,
                        keyfile=test_params.client_key_path)
    mqtt_client.connect(test_params.mqtt_host, test_params.mqtt_port, 60)
    mqtt_client.loop_start()
    print("waiting for mqtt connection..")
    connection_event.wait()
    yield mqtt_client
    print("stopping mqtt client..")
    mqtt_client.loop_stop()


@pytest.fixture(scope="session")
def cloudcam_client(test_params, mqtt_broker, gst_rtp_pipeline):
    print("launching cloudcam client..")
    with open(os.path.join(os.path.split(cloudcam_client_path)[0], 'config.json'), 'w') as f:
        json.dump({
            "thingName": test_params.thing_name,
            "thingTypeName": "Camera",
            "clientId": test_params.thing_name,
            "endpoint": test_params.mqtt_host,
            "caPem": open(test_params.ca_path, 'r').read(),
            "certificatePem": open(test_params.client_cert_path, 'r').read(),
            "certificatePrivateKey": open(test_params.client_key_path, 'r').read(),
            "rtsp": {
                "uri": "rtsp://" + test_params.rtsp_host + ":" + str(test_params.rtsp_port) + "/test"
            }
        }, f)
    client_proc = subprocess.Popen(cloudcam_client_path)
    # wait until client connects to mqtt broker and subscribes to relevant topics
    time.sleep(5)
    yield client_proc
    print("stopping cloudcam client..")
    client_proc.terminate()


@cherrypy.expose
class ThumbnailUploadService(object):
    """Provides a HTTP PUT endpoint for thumbnail uploads and uploaded data verification"""
    completion_event = threading.Event()

    def check_thumbnail(self, data):
        try:
            thumbnail = Image.open(data)
            print("check_thumbnail(): got %s of %s px" % (thumbnail.format, thumbnail.size))
            assert thumbnail.format == 'JPEG'
            self.completion_event.set()
        except Exception as e:
            # this is required here since cherrypy likes to catch our exceptions and turn them into HTTP 500 error pages
            pytest.fail("check_thumbnail(): %s" % e)

    def PUT(self, data):
        print('ThumbnailUploadService.PUT')
        self.check_thumbnail(cherrypy.request.body)


@pytest.fixture(scope="session")
def thumbnail_upload_service(test_params):
    print("starting cherrypy..")
    cherrypy.config.update({'server.socket_host': test_params.http_host,
                            'server.socket_port': test_params.http_port,
                            'engine.autoreload.on': False})
    conf = {
        '/thumbnail': {
            'request.dispatch': cherrypy.dispatch.MethodDispatcher()
        }
    }
    service = ThumbnailUploadService()
    cherrypy.tree.mount(service, '/', conf)
    cherrypy.engine.start()
    yield service
    print("stopping cherrypy..")
    cherrypy.engine.exit()


class GstMainLoopThread(threading.Thread):
    loop = GObject.MainLoop()
    gst_frame_count = 0
    completion_event = threading.Event()

    def run(self):
        self.loop.run()  # this can be function-scoped

    def gst_bus_call(self, bus, message, loop):
        t = message.type
        if t == Gst.MessageType.EOS:
            print("gst_bus_call(): end-of-stream\n")
            self.loop.quit()
        elif t == Gst.MessageType.ERROR:
            err, debug = message.parse_error()
            print("gst_bus_call(): error: %s: %s\n" % (err, debug))
            self.loop.quit()
        return True

    def gst_new_buffer(self, sink, data):
        sample = sink.emit("pull-sample")
        buf = sample.get_buffer()
        print("gst_new_buffer(): frame %d, timestamp %d" % (self.gst_frame_count, buf.pts))
        self.gst_frame_count += 1
        if self.gst_frame_count > 42:
            print("rtp stream test successful")
            self.completion_event.set()
        return Gst.FlowReturn.OK


@pytest.fixture(scope="session")
def gst_rtp_pipeline(test_params):
    print("starting gstreamer rtp pipeline..")
    GObject.threads_init()
    Gst.init(None)

    # setup a mock RTSP server
    rtsp_server = GstRtspServer.RTSPServer.new()
    rtsp_media_factory = GstRtspServer.RTSPMediaFactory.new()
    mounts = rtsp_server.get_mount_points()
    rtsp_server.set_service(str(test_params.rtsp_port))
    rtsp_media_factory.set_launch('videotestsrc ! \
		     video/x-raw,format=RGB,width=640,height=480,framerate=30/1 ! \
		     videoconvert ! x264enc ! video/x-h264,profile=baseline ! rtph264pay name=pay0 pt=96')
    mounts.add_factory('/test', rtsp_media_factory)
    rtsp_server.attach(None)

    # setup a pipeline which will receive RTP video and decode it, calling new_gst_buffer() on every decoded frame
    udpsrc = Gst.ElementFactory.make("udpsrc", None)
    udpsrc.set_property('port', test_params.rtp_port)
    udpsrc.set_property('caps', Gst.caps_from_string('application/x-rtp, encoding-name=H264, payload=96'))
    rtph264depay = Gst.ElementFactory.make("rtph264depay", None)
    h264parse = Gst.ElementFactory.make("h264parse", None)
    avdec_h264 = Gst.ElementFactory.make("avdec_h264", None)
    sink = Gst.ElementFactory.make("appsink", None)

    pipeline = Gst.Pipeline.new("rtp-pipeline")
    pipeline.add(udpsrc)
    pipeline.add(rtph264depay)
    pipeline.add(h264parse)
    pipeline.add(avdec_h264)
    pipeline.add(sink)

    udpsrc.link(rtph264depay)
    rtph264depay.link(h264parse)
    h264parse.link(avdec_h264)
    avdec_h264.link(sink)

    # gst event loop/bus
    loop_thread = GstMainLoopThread()

    sink.set_property("emit-signals", True)
    sink.connect("new-sample", loop_thread.gst_new_buffer, sink)

    bus = pipeline.get_bus()
    bus.add_signal_watch()
    bus.connect("message", loop_thread.gst_bus_call, loop_thread.loop)

    # start play back and listed to events
    pipeline.set_state(Gst.State.PLAYING)
    loop_thread.start()

    yield loop_thread

    print("stopping gstreamer rtp pipeline..")
    loop_thread.loop.quit()
    pipeline.set_state(Gst.State.NULL)


class TimeoutException(Exception):
    pass


def test_cloudcam_client(test_params, cloudcam_client):
    """Tests if cloudcam client could be started"""
    pass


def test_thumbnail_upload(test_params, cloudcam_client, mqtt_client, shadow_state, thumbnail_upload_service):
    """Tests thumbnail uploads by setting up a local http server and verifying that uploaded data is a valid JPEG image"""
    shadow_update_completion_event = threading.Event()
    # there's a listener at upload_url which receives and validates the jpeg thumbnail
    upload_url = 'http://%s:%d/thumbnail' % (test_params.http_host, test_params.http_port)
    # download_url is a dummy url
    download_url = 'http://%s:%d/thumbnail/%s' % (test_params.http_host, test_params.http_port, rand_string(20))

    # check if updated 'last_uploaded_thumb' shadow value is correct
    def check_last_uploaded_thumb(completion_event, value):
        assert value['status'] == 'success'
        assert value['upload_url'] == upload_url
        assert value['download_url'] == download_url
        print("check_last_uploaded_thumb(): ok")
        completion_event.set()

    shadow_state.shadow_handlers['last_uploaded_thumb'] = partial(check_last_uploaded_thumb,
                                                                  shadow_update_completion_event)
    # publish thumbnail upload command
    mqtt_client.publish(test_params.commands_topics, json.dumps({'command': 'upload_thumb',
                                                                 'upload_url': upload_url,
                                                                 'download_url': download_url}))
    # wait until both http upload and shadow status update happened
    if not thumbnail_upload_service.completion_event.wait(test_params.timeout):
        raise TimeoutException()
    if not shadow_update_completion_event.wait(test_params.timeout):
        raise TimeoutException()


def test_rtp_streaming(test_params, cloudcam_client, mqtt_client, shadow_state, gst_rtp_pipeline):
    """Tests thumbnail uploads by setting up a local http server and verifying that uploaded data is a valid JPEG image"""
    shadow_state.shadow_version += 1
    mqtt_client.publish(test_params.shadow_delta_topic, json.dumps({
        'state': {
            'streams': {
                'primary': {
                    'gateway_instance': test_params.rtp_host,
                    'stream_rtp_port': test_params.rtp_port,
                    'stream_h264_bitrate': 256
                },
                'current': 'primary'
            }
        },
        'version': shadow_state.shadow_version
    }))
    if not gst_rtp_pipeline.completion_event.wait(test_params.timeout):
        raise TimeoutException()
