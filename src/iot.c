// Routines for interfacing with AWS IoT

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "cloudcam/iot.h"
#include "cloudcam/log.h"

IoT_Error_t cloudcam_subscribe_topic(char *topic, iot_message_handler handler) {
  assert(topic);
  
  MQTTSubscribeParams sub_params = MQTTSubscribeParamsDefault;
  sub_params.mHandler = handler;
  sub_params.qos = QOS_0;
  sub_params.pTopic = topic;
  
  IoT_Error_t rc;
  INFO("Subscribing to thumbnail topic %s...", topic);
  rc = aws_iot_mqtt_subscribe(&sub_params);
  if (rc != NONE_ERROR)
    ERROR("Error subscribing");
  
  return rc;
}

// subscribe to topics we're interested in
void cloudcam_iot_subscribe() {
  cloudcam_subscribe_topic("cloudcam/thumb/request_snapshot", thumbnail_requested_handler);
}

// callback when server requests a new thumbnail
int thumbnail_requested_handler(MQTTCallbackParams params) {
  return 0;
}

// sit and wait for messages
void cloudcam_iot_poll_loop() {
  IoT_Error_t rc = NONE_ERROR;

  while (rc == NONE_ERROR) {
    sleep(1);
    rc = aws_iot_mqtt_yield(4000);
  }
  INFO("rc=%d\n", rc);
}
      
