#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <limits.h>
#include <fcntl.h>
#include <libgen.h>

// override logging with axis syslog logz
#include "cloudcam/log.h"
// AWS IoT interfacing
#include "cloudcam/iot.h"

char *dir;

void test_pub_thumb();
void test_pubsub();
void cleanup();

int MQTTcallbackHandler(MQTTCallbackParams params) {
  INFO("Subscribe callback");
  INFO("%.*s\t%.*s",
       (int)params.TopicNameLen, params.pTopicName,
       (int)params.MessageParams.PayloadLen, (char*)params.MessageParams.pPayload);

  return 0;
}

void disconnectCallbackHandler(void) {
  WARN("MQTT Disconnect");
}

int main(int argc, char** argv) {
  openlog("cloudcam", LOG_PID | LOG_CONS, LOG_USER);

  tolog(&stdout);
  tolog(&stderr);
  
  dir = dirname(argv[0]);
  INFO("current dir: %s\n", dir);
  
  IoT_Error_t rc = NONE_ERROR;
  MQTTClient_t mc;
  aws_iot_mqtt_init(&mc);
  
  char rootCA[PATH_MAX + 1];
  char clientCRT[PATH_MAX + 1];
  char clientKey[PATH_MAX + 1];
  char cafileName[] = AWS_IOT_ROOT_CA_FILENAME;
  char clientCRTName[] = AWS_IOT_CERTIFICATE_FILENAME;
  char clientKeyName[] = AWS_IOT_PRIVATE_KEY_FILENAME;

  INFO("\nAWS IoT SDK Version %d.%d.%d-%s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

  sprintf(rootCA, "%s/%s", dir, cafileName);
  sprintf(clientCRT, "%s/%s", dir, clientCRTName);
  sprintf(clientKey, "%s/%s", dir, clientKeyName);

  INFO("rootCA %s", rootCA);
  INFO("clientCRT %s", clientCRT);
  INFO("clientKey %s", clientKey);

  // TODO: verify cert files exist

  ShadowParameters_t sp = ShadowParametersDefault;
  sp.pMyThingName = AWS_IOT_MY_THING_NAME;
  sp.pMqttClientId = AWS_IOT_MQTT_CLIENT_ID;
  sp.pHost = AWS_IOT_MQTT_HOST;
  sp.port = AWS_IOT_MQTT_PORT;
  sp.pClientCRT = clientCRT;
  sp.pClientKey = clientKey;
  sp.pRootCA = rootCA;

  INFO("Shadow Init");
  rc = aws_iot_shadow_init(&mc);
  if (rc != NONE_ERROR)
    ERROR("Shadow init error %d", rc);

  INFO("Shadow Connect");
  rc = aws_iot_shadow_connect(&mc, &sp);
  if (rc != NONE_ERROR)
    ERROR("Shadow Connection Error %d", rc);

  INFO("Enabling auto-reconnect...");
  rc = mc.setAutoReconnectStatus(true);
  if (rc != NONE_ERROR) {
    ERROR("Error(%d) enabling auto-reconnect", rc);
    cleanup();
    exit(1);
  }

  // subscribe to topics and shadow deltas
  cloudcam_iot_subscribe(&mc);

  test_pub_thumb();
  cloudcam_iot_poll_loop(&mc);

  cleanup();
  return 0;
}

void cleanup() {
  closelog();
}

void test_pub_thumb() {
  struct stat file_stat;
  IoT_Error_t rc;
  MQTTMessageParams Msg = MQTTMessageParamsDefault;
  Msg.qos = QOS_0;
  MQTTPublishParams Params = MQTTPublishParamsDefault;
  Params.pTopic = "cloudcam/thumb/store";

  // read image
  int thumb_fh;
  char sample_file[PATH_MAX + 1];
  snprintf(sample_file, sizeof(sample_file), "%s/%s", dir, "sample/snowdino.jpg");
  thumb_fh = open(sample_file, O_RDONLY);
  if (thumb_fh == -1) {
    ERROR("Error opening %s: %s", sample_file, strerror(errno));
    return;
  }
  if (fstat(thumb_fh, &file_stat) != 0) {
    close(thumb_fh);
    ERROR("fstat error: %s", strerror(errno));
    return;
  }
  long long file_size = (long long)file_stat.st_size;
  uint8_t *img = (uint8_t *)malloc(file_size);
  ssize_t bytes_read = read(thumb_fh, img, file_size);
  if (bytes_read < file_size) {
    ERROR("failed to read in image file");
    close(thumb_fh);
    return;
  }
  printf("read image bytes: %lld\n", (long long)bytes_read);
  close(thumb_fh);

  // publish image
  char *t = "{\"abc\":123}";
  Msg.pPayload = t;
  Msg.PayloadLen = strlen(t) + 1;
  //  Msg.pPayload = (void *) img;
  //Msg.PayloadLen = 10;//bytes_read;

  struct timeval start,end;
  gettimeofday(&start, NULL);
  Params.MessageParams = Msg;
  // when this is published, a message will be sent back on
  // cloudcam/thumb/request_snapshot
  rc = aws_iot_mqtt_publish(&Params);
  gettimeofday(&end, NULL);

  long long sec = (long long)(end.tv_sec - start.tv_sec);
  long long usec = (long long)(end.tv_usec - start.tv_usec);
  printf("dur, sec: %lld, usec: %lld\n", sec, usec);
  
  if (rc == NONE_ERROR) {
    INFO("published to %s", Params.pTopic);
  } else {
    ERROR("failed to publish to topic %s, rv=%d", Params.pTopic, rc);
  }
}


