// override logging with axis syslog logz
#include <cloudcam/cloudcam.h>
#include "cloudcam/log.h"
// AWS IoT interfacing
#include "cloudcam/iot.h"

static int cloudcam_global_init();

static void cloudcam_global_cleanup();

static void cloudcam_init_logging();

int MQTTcallbackHandler(AWS_IoT_Client *iotc, char *topic_name, uint16_t topic_name_len,
                        IoT_Publish_Message_Params *params, void *data) {
  INFO("Subscribe callback");
  INFO("%.*s\t%.*s",
       (int)topic_name_len, topic_name,
       (int)params->payloadLen, (char *)params->payload);

  return 0;
}

void cloudcam_iot_disconnect_handler(AWS_IoT_Client *iotc, void *data) {
  WARN("MQTT Disconnect");
}

int main(int argc, char **argv) {
  // init test context
  cloudcam_ctx ctx;
  if (cloudcam_init_ctx(&ctx, argv[0]) != SUCCESS) {
    ERROR("Failed initializing CloudCam client context");
    return 1;
  }

  if (cloudcam_global_init() != SUCCESS) {
    ERROR("Failed to initialize all required components");
    return 1;
  }

  ccgst_start_stream(ctx.gst);

  IoT_Error_t rc = cloudcam_init_iot_client(&ctx);
  if (rc != SUCCESS) {
    WARN("cloudcam_init_iot_client failed, exiting")
    return rc;
  }

  if (cloudcam_connect_blocking(&ctx) != SUCCESS) {
    ERROR("Failed to connect to AWSIoT service");
    return 2;
  }

  while (1) {
    // mainloop
    rc = cloudcam_iot_poll_loop(&ctx);
    if (rc >= 0) {
      INFO("Exiting the main loop due to status %d", rc);
      break;
    }
    else if (rc != FAILURE) {
      // if rc indicates failure and it's not FAILURE (-1), which is returned on aws_iot_shadow_yield timeout (60s), try to reconnect
      cloudcam_connect_blocking(&ctx);
    }
  }

  // cleanup
  cloudcam_free_ctx(&ctx);
  cloudcam_global_cleanup();
  return 0;
}

// initialize a newly-allocated cloudcam context
// app_dir_path: root of application home dir, includes config dir
IoT_Error_t cloudcam_init_ctx(cloudcam_ctx *ctx, char *app_dir_path) {
  bzero(ctx, sizeof(cloudcam_ctx));  // set to all 0s

  // copy app path
  ctx->app_dir_path = strdup(app_dir_path);

  // alloc IoT client
  AWS_IoT_Client *iotc = malloc(sizeof(AWS_IoT_Client));
  ctx->iotc = iotc;

  ccgst_thread_ctx *gst = malloc(sizeof(ccgst_thread_ctx));
  ctx->gst = gst;

  return SUCCESS;
}

// block until client is connected
IoT_Error_t cloudcam_connect_blocking(cloudcam_ctx *ctx) {
  assert(ctx != NULL);
  assert(ctx->app_dir_path != NULL);

  IoT_Error_t rc;
  // attempt connect
  do {
    rc = cloudcam_iot_connect(ctx);
    if (rc != SUCCESS) {
      WARN("Failed to connect, will retry...")
      sleep(10);
    }
  } while (rc != SUCCESS);

  // should now be connected!

  // subscribe to topics and shadow deltas
  rc = cloudcam_iot_subscribe(ctx);
  if (rc != SUCCESS) {
    WARN("Failed to subscribe to topic");
    return rc;
  }

  return SUCCESS;
}

IoT_Error_t cloudcam_free_ctx(cloudcam_ctx *ctx) {
  free(ctx->app_dir_path);
  free(ctx->thing_name);
  free(ctx->commands_topic_name);
  free(ctx->client_id);
  free(ctx->endpoint);
  free(ctx->ca_path);
  free(ctx->client_crt_path);
  free(ctx->client_key_path);
  free(ctx->iotc);

  bzero(ctx, sizeof(cloudcam_ctx));

  return SUCCESS;
}

void cloudcam_init_logging() {
  openlog("cloudcam", LOG_PID | LOG_CONS, LOG_USER);

  tolog(&stdout);
  tolog(&stderr);
}

// main application setup
IoT_Error_t cloudcam_global_init() {
  cloudcam_init_logging();
  CURLcode curl_ret = curl_global_init(CURL_GLOBAL_SSL);
  if (curl_ret != CURLE_OK) {
    ERROR("Failed to initialize libcurl %d", curl_ret);
    return FAILURE;
  }
  return SUCCESS;
}

void cloudcam_global_cleanup() {
  closelog();
}
