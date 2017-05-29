// Routines for interfacing with AWS IoT

#include "cloudcam/iot.h"
#include "cloudcam/log.h"

const char *CLOUDCAM_TOPIC_THUMBNAIL_TEST = "cloudcam/thumb/store";

IoT_Error_t cloudcam_init_iot_client(cloudcam_ctx *ctx) {
  IoT_Error_t rc = SUCCESS;
  AWS_IoT_Client *iotc = ctx->iotc;

  char *dir = dirname(ctx->app_dir_path);
  INFO("current dir: %s\n", dir);
  INFO("\nAWS IoT SDK Version %d.%d.%d-%s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

  // load cert file paths
  char root_ca[PATH_MAX + 1];
  char client_crt[PATH_MAX + 1];
  char client_key[PATH_MAX + 1];
  char cafile_name[] = AWS_IOT_ROOT_CA_FILENAME;
  char client_crt_name[] = AWS_IOT_CERTIFICATE_FILENAME;
  char client_key_name[] = AWS_IOT_PRIVATE_KEY_FILENAME;
  sprintf(root_ca, "%s/%s", dir, cafile_name);
  sprintf(client_crt, "%s/%s", dir, client_crt_name);
  sprintf(client_key, "%s/%s", dir, client_key_name);
  INFO("root_ca %s", root_ca);
  INFO("client_crt %s", client_crt);
  INFO("client_key %s", client_key);

  // let's make sure these files exist
  if (access(client_crt, F_OK) == -1) {
    ERROR("Missing certificate file: %s", client_crt);
    return FAILURE;
  }
  if (access(client_key, F_OK) == -1) {
    ERROR("Missing key file: %s", client_key);
    return FAILURE;
  }

  // init shadow client and MQTT client
  ShadowInitParameters_t shadow_init_params = ShadowInitParametersDefault;
  shadow_init_params.pHost = AWS_IOT_MQTT_HOST;
  shadow_init_params.port = AWS_IOT_MQTT_PORT;
  shadow_init_params.pClientCRT = client_crt;
  shadow_init_params.pClientKey = client_key;
  shadow_init_params.pRootCA = root_ca;
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
  scp.mqttClientIdLen = (uint16_t)strlen(AWS_IOT_MQTT_CLIENT_ID);

  INFO("Shadow Connect");
  rc = aws_iot_shadow_connect(iotc, &scp);
  if (SUCCESS != rc) {
    ERROR("Shadow Connection Error: %d", rc);
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

IoT_Error_t cloudcam_subscribe_topic(cloudcam_ctx *ctx, char *topic, pApplicationHandler_t handler) {
  assert(topic);
    
  IoT_Error_t rc;
  INFO("Subscribing to thumbnail topic %s...", topic);
  rc = aws_iot_mqtt_subscribe(ctx->iotc, topic, strlen(topic), QOS1, handler, ctx);
  if (rc != SUCCESS) {
    ERROR("Error subscribing %d", rc);
  } else {
    INFO("Subscribed");
  }
  return rc;
}

// subscribe to topics we're interested in
IoT_Error_t cloudcam_iot_subscribe(cloudcam_ctx *ctx) {
  return cloudcam_subscribe_topic(ctx, "cloudcam/thumb/request_snapshot", thumbnail_requested_handler);

  // return;

  // static jsonStruct_t upload_shadow_update;
  // static char delta_buffer[SHADOW_MAX_SIZE_OF_RX_BUFFER];
  // upload_shadow_update.cb = shadow_delta_handler;
  // upload_shadow_update.pData = delta_buffer;
  // upload_shadow_update.pKey = "upload_endpoint";
  // upload_shadow_update.type = SHADOW_JSON_OBJECT;
  // IoT_Error_t rc = aws_iot_shadow_register_delta(iotc, &upload_shadow_update);
  // if (SUCCESS != rc) {
  //   ERROR("Shadow Register Delta Error");
  // } else {
  //   INFO("Registered for shadow delta updates");
  // }

  // return rc;
}

// sit and wait for messages
void cloudcam_iot_poll_loop(cloudcam_ctx *ctx) {
  IoT_Error_t rc = SUCCESS;

  while (rc == SUCCESS || rc == NETWORK_ATTEMPTING_RECONNECT || rc == NETWORK_RECONNECTED) {
    rc = aws_iot_shadow_yield(ctx->iotc, 200);
    if (rc == NETWORK_ATTEMPTING_RECONNECT) {
      sleep(1);
      continue;
    }
    if (rc != SUCCESS) {
      ERROR("Shadow yield error. Likely unexpected TCP socket disconnect. Error: %d", rc);
      sleep(2);
      break;
    }

    sleep(1);
  }
  INFO("Finished poll loop rc=%d\n", rc);
}
      
//////


// callback when server requests a new thumbnail
void thumbnail_requested_handler(AWS_IoT_Client *iotc, char *topic_name, unsigned short topic_name_len,
             IoT_Publish_Message_Params *params, void *user_data) {
  cloudcam_ctx *ctx = (cloudcam_ctx *)user_data;
  assert(ctx->iotc == iotc);
  // char *payload = strndup(params->payload, params->payloadLen);
  printf("thumb requested handler. payload:\n%s\n", params->payload);

  // parse payload JSON string
  json_error_t error;
  json_auto_t *root = json_loadb(params->payload, params->payloadLen, 0, &error);

  if (! root) {
      ERROR("error: on line %d: %s\n", error.line, error.text);
      return;
  }

  if (! json_is_object(root)) {
      ERROR("error: root is not an object\n");
      return;
  }

  json_auto_t *endpoint = json_object_get(root, "upload_endpoint");
  printf("got endpoint %s\n", json_string_value(endpoint));

  FILE *fp = fopen("sample/snowdino.jpg", "rb");
  if (!fp) {
      ERROR("error: thumb fopen failed\n");
      return;
  }
  cloudcam_upload_file_to_s3_presigned(ctx,fp,json_string_value(endpoint));
  fclose(fp);
}

// called when device shadow changes with info about what changed
void shadow_delta_handler(const char *json_str, uint32_t json_str_len, jsonStruct_t *js) {
  INFO("delta handler");
  if (json_str && js) {
    INFO("Delta - shadow state delta update: %s\n", json_str);
  }
}

