#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <syslog.h>
#include <limits.h>
#include <fcntl.h>

#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_interface.h"
#include "aws_iot_config.h"

#define LOGINFO(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOGERR(fmt, args...)     { syslog(LOG_CRIT, fmt, ## args); fprintf(stderr, fmt, ## args); }

void test_pub_thumb();
void test_pubsub();

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
  IoT_Error_t rc = NONE_ERROR;

  char rootCA[PATH_MAX + 1];
  char clientCRT[PATH_MAX + 1];
  char clientKey[PATH_MAX + 1];
  char CurrentWD[PATH_MAX + 1];
  char cafileName[] = AWS_IOT_ROOT_CA_FILENAME;
  char clientCRTName[] = AWS_IOT_CERTIFICATE_FILENAME;
  char clientKeyName[] = AWS_IOT_PRIVATE_KEY_FILENAME;

  INFO("\nAWS IoT SDK Version %d.%d.%d-%s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

  if (getcwd(CurrentWD, sizeof(CurrentWD)) == NULL) {
    ERROR("getcwd failed");
    return errno;
  }
  
  sprintf(rootCA, "%s/%s", CurrentWD, cafileName);
  sprintf(clientCRT, "%s/%s", CurrentWD, clientCRTName);
  sprintf(clientKey, "%s/%s", CurrentWD, clientKeyName);

  DEBUG("rootCA %s", rootCA);
  DEBUG("clientCRT %s", clientCRT);
  DEBUG("clientKey %s", clientKey);

  MQTTConnectParams connectParams = MQTTConnectParamsDefault;

  connectParams.KeepAliveInterval_sec = 10;
  connectParams.isCleansession = true;
  connectParams.MQTTVersion = MQTT_3_1_1;
  connectParams.pClientID = "CSDK-test-device";
  connectParams.pHostURL = AWS_IOT_MQTT_HOST;
  connectParams.port = AWS_IOT_MQTT_PORT;
  connectParams.isWillMsgPresent = false;
  connectParams.pRootCALocation = rootCA;
  connectParams.pDeviceCertLocation = clientCRT;
  connectParams.pDevicePrivateKeyLocation = clientKey;
  connectParams.mqttCommandTimeout_ms = 2000;
  connectParams.tlsHandshakeTimeout_ms = 5000;
  connectParams.isSSLHostnameVerify = true;// ensure this is set to true for production
  connectParams.disconnectHandler = disconnectCallbackHandler;

  INFO("Connecting...");
  rc = aws_iot_mqtt_connect(&connectParams);
  if (NONE_ERROR != rc) {
    ERROR("Error(%d) connecting to %s:%d", rc, connectParams.pHostURL, connectParams.port);
  }

  MQTTSubscribeParams subParams = MQTTSubscribeParamsDefault;
  subParams.mHandler = MQTTcallbackHandler;
  subParams.pTopic = "sdkTest/sub";
  subParams.qos = QOS_0;

  if (NONE_ERROR == rc) {
    INFO("Subscribing...");
    rc = aws_iot_mqtt_subscribe(&subParams);
    if (NONE_ERROR != rc) {
      ERROR("Error subscribing");
    }
  }

  /*********/
  test_pub_thumb();
  /*********/

  return 0;
}

void test_pubsub() {
  IoT_Error_t rc = NONE_ERROR;
  int publishCount = 5;
  int32_t i = 0;
  bool infinitePublishFlag = true;
  MQTTPublishParams Params = MQTTPublishParamsDefault;
  MQTTMessageParams Msg = MQTTMessageParamsDefault;
  Msg.qos = QOS_0;
  char cPayload[100];
  Params.pTopic = "sdkTest/sub";

  if(publishCount != 0){
    infinitePublishFlag = false;
  }

  while (NONE_ERROR == rc && (publishCount > 0 || infinitePublishFlag)) {
    //Max time the yield function will wait for read messages
    rc = aws_iot_mqtt_yield(100);
    INFO("-->sleep");
    sleep(1);
    sprintf(cPayload, "%s : %d ", "hello from SDK", i++);
    Msg.PayloadLen = strlen(cPayload) + 1;
    Params.MessageParams = Msg;
    rc = aws_iot_mqtt_publish(&Params);
    if(publishCount > 0){
      publishCount--;
    }
  }

  if (NONE_ERROR != rc){
    ERROR("An error occurred in the loop.\n");
  }
  else{
    INFO("Publish done\n");
  }
}

void test_pub_thumb() {
  struct stat file_stat;
  IoT_Error_t rc;
  MQTTPublishParams Params = MQTTPublishParamsDefault;
  Params.pTopic = "cloudcam/thumb/store";
  MQTTMessageParams Msg = MQTTMessageParamsDefault;
  Msg.qos = QOS_0;
  Params.MessageParams = Msg;

  // read image
  int thumb_fh;
  char sample_file[] = "sample/snowdino.jpg";
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
  Msg.pPayload = img;
  rc = aws_iot_mqtt_publish(&Params);
  if (rc == NONE_ERROR) {
    INFO("published to %s", Params.pTopic);
  } else {
    ERROR("failed to publish to topic %s", Params.pTopic);
  }
}
