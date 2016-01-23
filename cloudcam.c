#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <syslog.h>
#include <limits.h>

#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_interface.h"
#include "aws_iot_config.h"

#ifdef DEBUG
#define D(x)    x
#else
#define D(x)
#endif

#define LOGINFO(fmt, args...)    { syslog(LOG_INFO, fmt, ## args); printf(fmt, ## args); }
#define LOGERR(fmt, args...)     { syslog(LOG_CRIT, fmt, ## args); fprintf(stderr, fmt, ## args); }


int MQTTcallbackHandler(MQTTCallbackParams params) {
  LOGINFO("Subscribe callback");
  LOGINFO("%.*s\t%.*s",
       (int)params.TopicNameLen, params.pTopicName,
       (int)params.MessageParams.PayloadLen, (char*)params.MessageParams.pPayload);

  return 0;
}

void disconnectCallbackHandler(void) {
  WARN("MQTT Disconnect");
}

int main(int argc, char** argv) {
  int publishCount = 5;
  IoT_Error_t rc = NONE_ERROR;
  int32_t i = 0;
  bool infinitePublishFlag = true;

  char rootCA[PATH_MAX + 1];
  char clientCRT[PATH_MAX + 1];
  char clientKey[PATH_MAX + 1];
  char CurrentWD[PATH_MAX + 1];
  char cafileName[] = AWS_IOT_ROOT_CA_FILENAME;
  char clientCRTName[] = AWS_IOT_CERTIFICATE_FILENAME;
  char clientKeyName[] = AWS_IOT_PRIVATE_KEY_FILENAME;

  INFO("\nAWS IoT SDK Version %d.%d.%d-%s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

  getcwd(CurrentWD, sizeof(CurrentWD));
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

  MQTTMessageParams Msg = MQTTMessageParamsDefault;
  Msg.qos = QOS_0;
  char cPayload[100];
  sprintf(cPayload, "%s : %d ", "hello from SDK", i);
  Msg.pPayload = (void *) cPayload;


  MQTTPublishParams Params = MQTTPublishParamsDefault;
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

  return rc;
}

