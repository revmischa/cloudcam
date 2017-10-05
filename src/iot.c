// Routines for interfacing with AWS IoT

#include <cloudcam/cloudcam.h>
#include <signal.h>
#include <jansson.h>
#include <curl/curl.h>
#include "cloudcam/iot.h"
#include "cloudcam/s3.h"
#include "cloudcam/gst.h"
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
  AWS_IoT_Client *iotc = ctx->iotc;
  assert(iotc != NULL);
  char *dir = dirname(ctx->app_dir_path);
  INFO("current dir: %s\n", dir);
  INFO("\nAWS IoT SDK Version %d.%d.%d-%s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

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

  INFO("\nthing name: %s\nclient id: %s\nendpoint: %s\n", ctx->thing_name, ctx->client_id, ctx->endpoint);

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

  // init shadow client and MQTT client
  ShadowInitParameters_t shadow_init_params = ShadowInitParametersDefault;
  shadow_init_params.pHost = ctx->endpoint;
  shadow_init_params.port = AWS_IOT_MQTT_PORT;
  shadow_init_params.pClientCRT = ctx->client_crt_path;
  shadow_init_params.pClientKey = ctx->client_key_path;
  shadow_init_params.pRootCA = ctx->ca_path;
  shadow_init_params.enableAutoReconnect = false;
  shadow_init_params.disconnectHandler = cloudcam_iot_disconnect_handler;
  INFO("Shadow Init");
  IoT_Error_t rc = aws_iot_shadow_init(iotc, &shadow_init_params);
  if (rc != SUCCESS) {
    ERROR("Shadow init error %d", rc);
    return rc;
  }

  // shadow client connect
  ShadowConnectParameters_t scp = ShadowConnectParametersDefault;
  scp.pMyThingName = ctx->thing_name;
  scp.pMqttClientId = ctx->client_id;
  scp.mqttClientIdLen = (uint16_t)strlen(ctx->client_id);
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

// cloudcam_ctx pointer passed to shadow delta handlers
// needs to be global since IoT SDK doesn't currently allow us to pass context to the shadow delta callback
static cloudcam_ctx *delta_handler_ctx = NULL;

// subscribe to topics/shadow delta updates we're interested in
IoT_Error_t cloudcam_iot_subscribe(cloudcam_ctx *ctx) {
  assert(ctx != NULL);

  assert(delta_handler_ctx == NULL || delta_handler_ctx == ctx);
  delta_handler_ctx = ctx;

  // subscribe to thumb upload shadow deltas (thumb_upload key)
  static char thumb_upload_delta_buffer[SHADOW_MAX_SIZE_OF_RX_BUFFER];
  static jsonStruct_t thumb_upload_delta = {
    .cb = shadow_thumb_upload_delta_handler,
    .pData = thumb_upload_delta_buffer,
    .pKey = "thumb_upload",
    .type = SHADOW_JSON_OBJECT
  };

  IoT_Error_t rc = aws_iot_shadow_register_delta(ctx->iotc, &thumb_upload_delta);
  if (SUCCESS != rc) {
    ERROR("Shadow Register Delta Error");
  } else {
    INFO("Registered for thumb_upload shadow delta updates");
  }

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
    ERROR("Shadow Register Delta Error");
  } else {
    INFO("Registered for streams shadow delta updates");
  }
  return rc;
}

// sit and wait for messages
void cloudcam_iot_poll_loop(cloudcam_ctx *ctx) {
  assert(ctx != NULL);

  IoT_Error_t rc;
  do {
    rc = aws_iot_shadow_yield(ctx->iotc, 200);
    if (rc == NETWORK_ATTEMPTING_RECONNECT) {
      INFO("Attempting reconnect\n");
      sleep(1);
      continue;
    }
    if (rc < 0) {
      ERROR("Shadow yield error. Likely unexpected TCP socket disconnect. Error: %d", rc);
      sleep(2);
      break;
    }
    sleep(1);
  } while (rc >= 0);

  INFO("Finished poll loop rc=%d\n", rc);
}

void shadow_update_callback(const char *pThingName, ShadowActions_t action, Shadow_Ack_Status_t status,
                            const char *pReceivedJsonDocument, void *pContextData) {
  assert(pThingName != NULL);
  assert(pReceivedJsonDocument != NULL);
  //cloudcam_ctx *ctx = (cloudcam_ctx *)pContextData;
  //assert(ctx != NULL);
  INFO("shadow_update_callback(%s, %d, %d): received %s\n", pThingName, action, status, pReceivedJsonDocument);
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

// thumb upload delta handler (thumb_upload key)
void shadow_thumb_upload_delta_handler(const char *json_str, uint32_t json_str_len, jsonStruct_t *js) {
  cloudcam_ctx *ctx = delta_handler_ctx;
  assert(ctx != NULL);

  INFO("shadow_thumb_upload_delta_handler");

  if (json_str && json_str_len > 0 && js) {
    // parse json under "upload_url" key
    json_error_t json_error;
    json_t *root = json_loadb(json_str, json_str_len, 0, &json_error);
    if (root == NULL) {
      ERROR("error: payload parsing error: %s at %s\n", json_error.text, json_error.source);
      return;
    }
    json_t *upload_url = json_object_get(root, "upload_url");
    if (upload_url == NULL) {
      ERROR("error: payload missing 'upload_url' key\n");
      return;
    }
    json_t *download_url = json_object_get(root, "download_url");
    if (download_url == NULL) {
      ERROR("error: payload missing 'download_url' key\n");
      return;
    }
    INFO("Delta - shadow state delta update: download_url %s\n", json_string_value(download_url));

    IoT_Error_t rc = 0;

    // upload stream snapshot to s3
    size_t snapshot_size = 0;
    void *snapshot_data = gst_get_jpeg_snapshot(ctx->gst, &snapshot_size);
    if (snapshot_data != NULL && snapshot_size != 0) {
      rc = cloudcam_upload_buffer_to_s3_presigned(ctx, snapshot_data, snapshot_size, json_string_value(upload_url));
    }
    free(snapshot_data);

    // update device shadow with the result
    char json_buf[AWS_IOT_MQTT_TX_BUF_LEN];
    snprintf(json_buf, sizeof(json_buf),
             "{\"state\":{\"reported\":{\"thumb_upload\":{\"upload_url\":\"%s\",\"download_url\":\"%s\",\"status\":\"%s\",\"timestamp\":%ld}}}}",
             json_string_value(upload_url), json_string_value(download_url), rc < 0 ? "failure" : "success",
             (unsigned long)time(NULL));
    INFO("shadow update json: %s\n", json_buf);
    rc = aws_iot_shadow_update(ctx->iotc, ctx->thing_name, json_buf, shadow_update_callback, ctx, 30, 1);
    if (rc < 0) {
      ERROR("Shadow update error: %d", rc);
    }

    // release json object
    json_decref(root);
  }
}

// a/v streams delta handler (streams key)
void shadow_streams_delta_handler(const char *json_str, uint32_t json_str_len, jsonStruct_t *js) {
  cloudcam_ctx *ctx = delta_handler_ctx;
  assert(ctx != NULL);

  INFO("shadow_streams_delta_handler");
  INFO("%.*s", json_str_len, json_str);

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
      INFO("Delta - shadow state delta update: gateway_instance %s, stream_rtp_port %lld, stream_h264_bitrate %lld\n",
           json_string_value(gateway_instance), json_integer_value(stream_rtp_port),
           stream_h264_bitrate ? json_integer_value(stream_h264_bitrate) : 0);
      int h264_bitrate = stream_h264_bitrate ? json_integer_value(stream_h264_bitrate) : default_h264_bitrate;
      const char *rtp_host = json_string_value(gateway_instance);
      int rtp_port = json_integer_value(stream_rtp_port);

      gst_update_stream_params(ctx->gst, h264_bitrate, rtp_host, rtp_port);
    }
    else {
      gst_update_stream_params(ctx->gst, default_h264_bitrate, "localhost", 20000);
    }
    // release json object
    json_decref(root);
  }
}

