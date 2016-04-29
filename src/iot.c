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
void cloudcam_iot_subscribe(MQTTClient_t *mc) {
  cloudcam_subscribe_topic("cloudcam/thumb/request_snapshot", thumbnail_requested_handler);

  static jsonStruct_t upload_shadow_update;
  static char delta_buffer[SHADOW_MAX_SIZE_OF_RX_BUFFER];
  upload_shadow_update.cb = shadow_delta_handler;
  upload_shadow_update.pData = delta_buffer;
  upload_shadow_update.pKey = "upload_endpoint";
  upload_shadow_update.type = SHADOW_JSON_OBJECT;
  IoT_Error_t rc = aws_iot_shadow_register_delta(mc, &upload_shadow_update);
  if (NONE_ERROR != rc) {
    ERROR("Shadow Register Delta Error");
  } else {
    INFO("Registered for shadow delta updates");
  }

  sleep(1);
}

// sit and wait for messages
void cloudcam_iot_poll_loop(MQTTClient_t *mc) {
  IoT_Error_t rc = NONE_ERROR;

  while (rc == NONE_ERROR || rc == NETWORK_ATTEMPTING_RECONNECT || rc == RECONNECT_SUCCESSFUL) {
    rc = aws_iot_shadow_yield(mc, 200);
    if (NETWORK_ATTEMPTING_RECONNECT == rc) {
      sleep(1);
      continue;
    }
    if (rc == YIELD_ERROR) {
      ERROR("Shadow yield error. Likely unexpected TCP socket disconnect.");
      break;
    }

    sleep(1);
  }
  INFO("rc=%d\n", rc);
}
      
//////


// callback when server requests a new thumbnail
int thumbnail_requested_handler(MQTTCallbackParams params) {
  printf("thumb requested handler\n");
  return 0;
}

void shadow_delta_handler(const char *json_str, uint32_t json_str_len, jsonStruct_t *js) {
  INFO("delta handler");
  if (json_str && js) {
    INFO("Delta - shadow state delta update: %s\n", json_str);
  }
}

