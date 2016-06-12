// Routines for interfacing with AWS IoT

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <libgen.h>

#include "cloudcam/iot.h"
#include "cloudcam/log.h"

const char *CLOUDCAM_TOPIC_THUMBNAIL_TEST = "cloudcam/thumb/store";

IoT_Error_t cloudcam_init_iot_client(AWS_IoT_Client *iotc, char *app_path) {
  IoT_Error_t rc = SUCCESS;

  char *dir = dirname(app_path);
  INFO("current dir: %s\n", dir);

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

  // init shadow client and MQTT client
  ShadowInitParameters_t shadow_init_params = ShadowInitParametersDefault;
  shadow_init_params.pHost = AWS_IOT_MQTT_HOST;
  shadow_init_params.port = AWS_IOT_MQTT_PORT;
  shadow_init_params.pClientCRT = clientCRT;
  shadow_init_params.pClientKey = clientKey;
  shadow_init_params.pRootCA = rootCA;
  shadow_init_params.enableAutoReconnect = false;
  shadow_init_params.disconnectHandler = cloudcam_iot_disconnect_handler;
  INFO("Shadow Init");
  rc = aws_iot_shadow_init(iotc, &shadow_init_params);
  if (rc != SUCCESS) {
    ERROR("Shadow init error %d", rc);
    return rc;
  }

  // shadow client connect
  ShadowConnectParameters_t scp = ShadowConnectParametersDefault;
  scp.pMyThingName = AWS_IOT_MY_THING_NAME;
  scp.pMqttClientId = AWS_IOT_MQTT_CLIENT_ID;

  INFO("Shadow Connect");
  rc = aws_iot_shadow_connect(iotc, &scp);
  if (SUCCESS != rc) {
    ERROR("Shadow Connection Error");
    return rc;
  }

  /*
   * Enable Auto Reconnect functionality. Minimum and Maximum time of Exponential backoff are set in aws_iot_config.h
   *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
   *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
   */
  rc = aws_iot_shadow_set_autoreconnect_status(iotc, true);
  if (SUCCESS != rc) {
    ERROR("Unable to set Auto Reconnect to true - %d", rc);
    return rc;
  }

  return SUCCESS;
}

IoT_Error_t cloudcam_subscribe_topic(AWS_IoT_Client *iotc, char *topic, pApplicationHandler_t handler) {
  assert(topic);
    
  IoT_Error_t rc;
  INFO("Subscribing to thumbnail topic %s...", topic);
  rc = aws_iot_mqtt_subscribe(iotc, topic, strlen(topic), QOS0, handler, NULL);
  if (rc != SUCCESS)
    ERROR("Error subscribing");
  
  return rc;
}

// subscribe to topics we're interested in
void cloudcam_iot_subscribe(AWS_IoT_Client *iotc) {
  cloudcam_subscribe_topic(iotc, "cloudcam/thumb/request_snapshot", thumbnail_requested_handler);

  static jsonStruct_t upload_shadow_update;
  static char delta_buffer[SHADOW_MAX_SIZE_OF_RX_BUFFER];
  upload_shadow_update.cb = shadow_delta_handler;
  upload_shadow_update.pData = delta_buffer;
  upload_shadow_update.pKey = "upload_endpoint";
  upload_shadow_update.type = SHADOW_JSON_OBJECT;
  IoT_Error_t rc = aws_iot_shadow_register_delta(iotc, &upload_shadow_update);
  if (SUCCESS != rc) {
    ERROR("Shadow Register Delta Error");
  } else {
    INFO("Registered for shadow delta updates");
  }

  sleep(1);
}

// sit and wait for messages
void cloudcam_iot_poll_loop(AWS_IoT_Client *iotc) {
  IoT_Error_t rc = SUCCESS;

  while (rc == SUCCESS || rc == NETWORK_ATTEMPTING_RECONNECT || rc == NETWORK_RECONNECTED) {
    rc = aws_iot_shadow_yield(iotc, 200);
    if (rc == NETWORK_ATTEMPTING_RECONNECT) {
      sleep(1);
      continue;
    }
    if (rc != SUCCESS) {
      ERROR("Shadow yield error. Likely unexpected TCP socket disconnect. Error: %d", rc);
      break;
    }

    sleep(1);
  }
  INFO("rc=%d\n", rc);
}
      
//////


// callback when server requests a new thumbnail
void thumbnail_requested_handler(AWS_IoT_Client *iotc, char *topic_name, unsigned short topic_name_len,
             IoT_Publish_Message_Params *params, void *data) {
  printf("thumb requested handler\n");
}

void shadow_delta_handler(const char *json_str, uint32_t json_str_len, jsonStruct_t *js) {
  INFO("delta handler");
  if (json_str && js) {
    INFO("Delta - shadow state delta update: %s\n", json_str);
  }
}

