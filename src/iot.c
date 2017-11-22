// Routines for interfacing with AWS IoT

#include <cloudcam/cloudcam.h>
#include <signal.h>
#include <jansson.h>
#include <curl/curl.h>
#include "cloudcam/iot.h"
#include "cloudcam/s3.h"
#include "cloudcam/ccgst.h"
#include "../config/aws_iot_config.h"

// writes a string to the specified file, creating it if necessary
IoT_Error_t spit(const char *path, const char *str) {
  FILE *f = fopen(path, "wb");
  if (f == NULL) {
    ERROR("failed to open %s for writing (%d)\n", path, errno);
    return FAILURE;
  }
  size_t str_length = strlen(str);
  if (fwrite(str, 1, str_length, f) != str_length) {
    ERROR("failed to write to %s (%d)\n", path, errno);
    fclose(f);
    return FAILURE;
  }
  fclose(f);
  return SUCCESS;
}

IoT_Error_t cloudcam_init_iot_client(cloudcam_ctx *ctx) {
  assert(ctx != NULL);
  char *dir = dirname(ctx->app_dir_path);
  DEBUG("current dir: %s\n", dir);
  DEBUG("\nAWS IoT SDK Version %d.%d.%d-%s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

  char config_path[PATH_MAX + 1];
  snprintf(config_path, sizeof(config_path), "%s/%s", dir, AWS_IOT_CONFIG_FILENAME);

  // read the config file containing thing configuration and keys/certs (produced by iot_provision_thing lambda function)
  json_error_t json_error;
  json_t *config_root = json_load_file(config_path, 0, &json_error);
  if (config_root == NULL) {
    ERROR("error: payload parsing config file %s: %s at %s\n", config_path, json_error.text, json_error.source);
    return FAILURE;
  }
  json_t *thing_name = json_object_get(config_root, "thingName");
  json_t *client_id = json_object_get(config_root, "clientId");
  json_t *endpoint = json_object_get(config_root, "endpoint");
  json_t *ca_pem = json_object_get(config_root, "caPem");
  json_t *certificate_pem = json_object_get(config_root, "certificatePem");
  json_t *certificate_private_key = json_object_get(config_root, "certificatePrivateKey");
  if (thing_name == NULL || client_id == NULL || endpoint == NULL || ca_pem == NULL ||
      certificate_pem == NULL || certificate_private_key == NULL) {
    ERROR("error: invalid config file format; verify all required keys (thingName, clientId, endpoint, "
            "caPem, certificatePem, certificatePrivateKey) are present\n");
    return FAILURE;
  }

  ctx->thing_name = strdup(json_string_value(thing_name));
  ctx->client_id = strdup(json_string_value(client_id));
  ctx->endpoint = strdup(json_string_value(endpoint));

  DEBUG("\nthing name: %s\nclient id: %s\nendpoint: %s\n", ctx->thing_name, ctx->client_id, ctx->endpoint);

  ctx->commands_topic_name = malloc(256);
  snprintf(ctx->commands_topic_name, 256, "cloudcam/%s/commands", ctx->thing_name);

  ctx->ca_path = malloc(PATH_MAX);
  snprintf(ctx->ca_path, PATH_MAX, "%s/%s", dir, AWS_IOT_ROOT_CA_FILENAME);

  ctx->client_crt_path = malloc(PATH_MAX);
  snprintf(ctx->client_crt_path, PATH_MAX, "%s/%s", dir, AWS_IOT_CERTIFICATE_FILENAME);

  ctx->client_key_path = malloc(PATH_MAX);
  snprintf(ctx->client_key_path, PATH_MAX, "%s/%s", dir, AWS_IOT_PRIVATE_KEY_FILENAME);

  // write out certificates/keys from the config into separate files for the IoT SDK convenience
  if (spit(ctx->ca_path, json_string_value(ca_pem)) != SUCCESS ||
      spit(ctx->client_crt_path, json_string_value(certificate_pem)) != SUCCESS ||
      spit(ctx->client_key_path, json_string_value(certificate_private_key)) != SUCCESS) {
    ERROR("Unable to write cert/key files");
    return FAILURE;
  }

  // release the config_root
  json_decref(config_root);

  return SUCCESS;
}

IoT_Error_t cloudcam_iot_connect(cloudcam_ctx *ctx) {
  assert(ctx != NULL);

  // init shadow client and MQTT client
  ShadowInitParameters_t shadow_init_params = ShadowInitParametersDefault;
  shadow_init_params.pHost = ctx->endpoint;
  shadow_init_params.port = AWS_IOT_MQTT_PORT;
  shadow_init_params.pClientCRT = ctx->client_crt_path;
  shadow_init_params.pClientKey = ctx->client_key_path;
  shadow_init_params.pRootCA = ctx->ca_path;
  shadow_init_params.enableAutoReconnect = false;
  shadow_init_params.disconnectHandler = cloudcam_iot_disconnect_handler;
  DEBUG("Shadow Init");
  IoT_Error_t rc = aws_iot_shadow_init(ctx->iotc, &shadow_init_params);
  if (rc != SUCCESS) {
    ERROR("Shadow init error %d", rc);
    return rc;
  }

  // shadow client connect
  ShadowConnectParameters_t scp = ShadowConnectParametersDefault;
  scp.pMyThingName = ctx->thing_name;
  scp.pMqttClientId = ctx->client_id;
  scp.mqttClientIdLen = (uint16_t)strlen(ctx->client_id);
  DEBUG("Shadow Connect");
  rc = aws_iot_shadow_connect(ctx->iotc, &scp);
  if (SUCCESS != rc) {
    ERROR("Shadow Connection Error: %d", rc);
    return rc;
  }
  /*
   * Enable Auto Reconnect functionality. Minimum and Maximum time of Exponential backoff are set in aws_iot_config.h
   *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
   *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
   */
  rc = aws_iot_shadow_set_autoreconnect_status(ctx->iotc, true);
  if (SUCCESS != rc) {
    ERROR("Unable to set Auto Reconnect to true - %d", rc);
    return rc;
  }
  DEBUG("cloudcam_iot_connect: done");
  return SUCCESS;
}

// cloudcam_ctx pointer passed to shadow delta handlers
// needs to be global since IoT SDK doesn't currently allow us to pass context to the shadow delta callback
static cloudcam_ctx *delta_handler_ctx = NULL;

// subscribe to topics/shadow delta updates we're interested in
IoT_Error_t cloudcam_iot_subscribe(cloudcam_ctx *ctx) {
  assert(ctx != NULL);

  // subscribe to commands topic
  DEBUG("Subscribing to %s", ctx->commands_topic_name);
  IoT_Error_t rc = aws_iot_mqtt_subscribe(ctx->iotc, ctx->commands_topic_name, strlen(ctx->commands_topic_name), 1, &cloudcam_iot_command_handler, (void*)ctx);
  if (SUCCESS != rc) {
    ERROR("Failed to subscribe to commands topic (%d)", rc);
    return rc;
  }

  // assign delta_handler_ctx (see above on why it is a static variable)
  assert(delta_handler_ctx == NULL || delta_handler_ctx == ctx);
  delta_handler_ctx = ctx;

  // subscribe to a/v streams shadow deltas (streams key)
  static char streams_delta_buffer[SHADOW_MAX_SIZE_OF_RX_BUFFER];
  static jsonStruct_t streams_delta = {
    .cb = shadow_streams_delta_handler,
    .pData = streams_delta_buffer,
    .pKey = "streams",
    .type = SHADOW_JSON_OBJECT
  };

  rc = aws_iot_shadow_register_delta(ctx->iotc, &streams_delta);
  if (SUCCESS != rc) {
    ERROR("Shadow Register Delta Error (%d)", rc);
  } else {
    DEBUG("Registered for streams shadow delta updates");
  }
  return rc;
}

// sit and wait for messages
IoT_Error_t cloudcam_iot_poll_loop(cloudcam_ctx *ctx) {
  assert(ctx != NULL);

  IoT_Error_t rc;
  do {
    rc = aws_iot_shadow_yield(ctx->iotc, 5000);
    if (rc == NETWORK_ATTEMPTING_RECONNECT) {
      DEBUG("Attempting reconnect\n");
      sleep(1);
      continue;
    }
    else if (rc == FAILURE) {
      DEBUG("aws_iot_shadow_yield timeout\n");
      sleep(1);
      continue;
    }
    else if (rc < 0) {
      ERROR("Shadow yield error. Likely unexpected TCP socket disconnect. Error: %d", rc);
      sleep(2);
      break;
    }
    sleep(1);
  } while (rc >= 0);

  DEBUG("Finished poll loop rc=%d\n", rc);

  return rc;
}

void shadow_update_callback(const char *pThingName, ShadowActions_t action, Shadow_Ack_Status_t status,
                            const char *pReceivedJsonDocument, void *pContextData) {
  assert(pThingName != NULL);
  assert(pReceivedJsonDocument != NULL);
  //cloudcam_ctx *ctx = (cloudcam_ctx *)pContextData;
  //assert(ctx != NULL);
  DEBUG("shadow_update_callback(%s, %d, %d): received %s\n", pThingName, action, status, pReceivedJsonDocument);
}

// downloads an url to a specified file (via curl)
IoT_Error_t download_file(const char *url, const char *path) {
  CURL *curl = curl_easy_init();
  assert(curl != NULL);

  FILE *fp = fopen(path, "wb");

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

  CURLcode rc = curl_easy_perform(curl);

  curl_easy_cleanup(curl);
  fclose(fp);

  if (rc != CURLE_OK) {
    ERROR("File download failed: %s", curl_easy_strerror(rc));
    return FAILURE;
  }

  return SUCCESS;
}

static void thumb_upload_command_handler(cloudcam_ctx *ctx, json_t *root) {
  json_t *upload_url = json_object_get(root, "upload_url");
  if (upload_url == NULL || !json_is_string(upload_url)) {
    ERROR("error: payload missing string 'upload_url' key\n");
    return;
  }
  json_t *download_url = json_object_get(root, "download_url");
  if (download_url == NULL || !json_is_string(download_url)) {
    ERROR("error: payload missing string 'download_url' key\n");
    return;
  }

  DEBUG("thumb_upload_command_handler: download_url %s\n", json_string_value(download_url));
  IoT_Error_t rc = 0;

  // upload stream snapshot to s3
  size_t snapshot_size = 0;
  void *snapshot_data = ccgst_get_jpeg_snapshot(ctx->gst, &snapshot_size);
  if (snapshot_data != NULL && snapshot_size != 0) {
    rc = cloudcam_upload_buffer_to_s3_presigned(ctx, snapshot_data, snapshot_size, json_string_value(upload_url));
  }
  free(snapshot_data);

  // update device shadow with the result
  char json_buf[AWS_IOT_MQTT_TX_BUF_LEN];
  snprintf(json_buf, sizeof(json_buf),
           "{\"state\":{\"reported\":{\"last_uploaded_thumb\":{\"upload_url\":\"%s\",\"download_url\":\"%s\",\"status\":\"%s\",\"timestamp\":%ld}}}}",
           json_string_value(upload_url), json_string_value(download_url), rc < 0 ? "failure" : "success",
           (unsigned long)time(NULL));
  DEBUG("shadow update json: %s\n", json_buf);
  rc = aws_iot_shadow_update(ctx->iotc, ctx->thing_name, json_buf, shadow_update_callback, ctx, 30, 1);
  if (rc < 0) {
    ERROR("Shadow update error: %d", rc);
  }
}

// cloudcam iot command handler
void cloudcam_iot_command_handler(AWS_IoT_Client *pClient, char *pTopicName, uint16_t topicNameLen, IoT_Publish_Message_Params *pParams, void *pClientData) {
  cloudcam_ctx *ctx = (cloudcam_ctx *)pClientData;
  assert(ctx != NULL);
  assert(pParams != NULL);
  assert(pParams->payload != NULL);

  DEBUG("cloudcam_iot_command_handler");

  if (pParams->payloadLen > 0) {
    // parse json
    json_error_t json_error;
    json_t *root = json_loadb(pParams->payload, pParams->payloadLen, 0, &json_error);
    if (root == NULL) {
      ERROR("error: payload parsing error: %s at %s\n", json_error.text, json_error.source);
      return;
    }
    json_t *command = json_object_get(root, "command");
    if (command == NULL || !json_is_string(command)) {
      ERROR("error: payload missing string 'command' key\n");
      // release json object
      json_decref(root);
      return;
    }
    DEBUG("cloudcam_iot_command_handler: %s", json_string_value(command));
    // call the command handler
    if (strcmp(json_string_value(command), "upload_thumb") == 0) {
      thumb_upload_command_handler(ctx, root);
    }
    else {
      ERROR("error: unknown iot command %s\n", json_string_value(command));
    }
    // release json object
    json_decref(root);
  }

  DEBUG("cloudcam_iot_command_handler: done");
}

// a/v streams delta handler (streams key)
void shadow_streams_delta_handler(const char *json_str, uint32_t json_str_len, jsonStruct_t *js) {
  cloudcam_ctx *ctx = delta_handler_ctx;
  assert(ctx != NULL);

  DEBUG("shadow_streams_delta_handler");
  DEBUG("%.*s", json_str_len, json_str);

  const int default_h264_bitrate = 256;

  if (json_str && json_str_len > 0 && js) {
    json_error_t json_error;
    json_t *root = json_loadb(json_str, json_str_len, 0, &json_error);
    if (root == NULL) {
      ERROR("error: payload parsing error: %s at %s\n", json_error.text, json_error.source);
      return;
    }
    json_t *current = json_object_get(root, "current");
    if (current && !json_is_null(current)) {
      // parse the stream configuration
      json_t *session_root = NULL;
      if (json_is_string(current) && strcmp("primary", json_string_value(current)) == 0) {
        session_root = json_object_get(root, "primary");
      } else if (json_is_string(current) && strcmp("standby", json_string_value(current)) == 0) {
        session_root = json_object_get(root, "standby");
      } else {
        ERROR("error: invalid value at the 'streams.current' node\n");
        return;
      }
      json_t *gateway_instance = json_object_get(session_root, "gateway_instance");
      if (gateway_instance == NULL) {
        ERROR("error: payload missing 'gateway_instance' key\n");
        return;
      }
      json_t *stream_rtp_port = json_object_get(session_root, "stream_rtp_port");
      if (stream_rtp_port == NULL) {
        ERROR("error: payload missing 'stream_rtp_port' key\n");
        return;
      }
      json_t *stream_h264_bitrate = json_object_get(session_root, "stream_h264_bitrate");
      DEBUG("Delta - shadow state delta update: gateway_instance %s, stream_rtp_port %lld, stream_h264_bitrate %lld\n",
           json_string_value(gateway_instance), json_integer_value(stream_rtp_port),
           stream_h264_bitrate ? json_integer_value(stream_h264_bitrate) : 0);
      int h264_bitrate = stream_h264_bitrate ? json_integer_value(stream_h264_bitrate) : default_h264_bitrate;
      const char *rtp_host = json_string_value(gateway_instance);
      int rtp_port = json_integer_value(stream_rtp_port);

      ccgst_update_stream_params(ctx->gst, h264_bitrate, rtp_host, rtp_port);
    }
    else {
      ccgst_update_stream_params(ctx->gst, default_h264_bitrate, "localhost", 20000);
    }
    // todo: update the shadow with actual gateway params
//    char json_buf[AWS_IOT_MQTT_TX_BUF_LEN];
//    snprintf(json_buf, sizeof(json_buf),
//             "{\"state\":{\"reported\":{\"thumb_upload\":{\"upload_url\":\"%s\",\"download_url\":\"%s\",\"status\":\"%s\",\"timestamp\":%ld}}}}",
//             ctx->gst->h264_bitrate, ctx->gst->rtp_host, ctx->gst->rtp_port);
//    DEBUG("shadow update json: %s\n", json_buf);
//    rc = aws_iot_shadow_update(ctx->iotc, ctx->thing_name, json_buf, shadow_update_callback, ctx, 30, 1);
//    if (rc < 0) {
//      ERROR("Shadow update error: %d", rc);
//    }

    // release json object
    json_decref(root);
  }
}

