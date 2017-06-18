// Routines for interfacing with AWS IoT

#include <cloudcam/cloudcam.h>
#include <jansson.h>
#include <curl/curl.h>
#include "cloudcam/iot.h"
#include "cloudcam/s3.h"
#include "../config/aws_iot_config.h"

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

  // read the config file (produced by iot_provision_thing lambda)
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

// needs to be global since IoT SDK doesn't currently allow us to pass context to the shadow delta callback
static cloudcam_ctx *delta_handler_ctx = NULL;

// subscribe to topics we're interested in
IoT_Error_t cloudcam_iot_subscribe(cloudcam_ctx *ctx) {
  assert(ctx != NULL);

  static char delta_buffer[SHADOW_MAX_SIZE_OF_RX_BUFFER];
  static jsonStruct_t delta = {
    .cb = thumb_upload_handler,
    .pData = delta_buffer,
    .pKey = "thumb_upload",
    .type = SHADOW_JSON_OBJECT
  };

  assert(delta_handler_ctx == NULL || delta_handler_ctx == ctx);
  delta_handler_ctx = ctx;

  IoT_Error_t rc = aws_iot_shadow_register_delta(ctx->iotc, &delta);
  if (SUCCESS != rc) {
    ERROR("Shadow Register Delta Error");
  } else {
    INFO("Registered for shadow delta updates");
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

// called when device shadow changes with info about what changed
void thumb_upload_handler(const char *json_str, uint32_t json_str_len, jsonStruct_t *js) {
  cloudcam_ctx *ctx = delta_handler_ctx;
  assert(ctx != NULL);

  INFO("thumb_upload_handler handler");

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

    // since we don't yet support grabbing actual thumbnails from the camera, get a random picture off the Internet
    const char *thumbnail_path = "thumb.jpg";
    if (download_file("http://lorempixel.com/200/200/nature/", thumbnail_path) != SUCCESS) {
      thumbnail_path = "sample/snowdino.jpg";
    }

    // upload to s3
    FILE *fp = fopen(thumbnail_path, "rb");
    if (!fp) {
      ERROR("error: thumb fopen failed\n");
      return;
    }
    IoT_Error_t rc = cloudcam_upload_file_to_s3_presigned(ctx, fp, json_string_value(upload_url));
    fclose(fp);

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

