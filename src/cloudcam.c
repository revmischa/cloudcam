
// override logging with axis syslog logz
#include "cloudcam/log.h"
// AWS IoT interfacing
#include "cloudcam/iot.h"

static void test_pub_thumb(AWS_IoT_Client *iotc);
static int cloudcam_global_init();
static void cloudcam_global_cleanup();
static void cloudcam_init_logging();

int MQTTcallbackHandler(AWS_IoT_Client *iotc, char *topic_name, uint16_t topic_name_len, IoT_Publish_Message_Params *params, void *data) {
  INFO("Subscribe callback");
  INFO("%.*s\t%.*s",
       (int)topic_name_len, topic_name,
       (int)params->payloadLen, (char*)params->payload);

  return 0;
}

void cloudcam_iot_disconnect_handler(AWS_IoT_Client *iotc, void *data) {
  WARN("MQTT Disconnect");
}

int main(int argc, char** argv) {
  cloudcam_ctx ctx;
  if (cloudcam_init_ctx(&ctx, argv[0]) != SUCCESS) {
    ERROR("Failed initializing CloudCam client context");
    return 1;
  }

  if (cloudcam_global_init() != SUCCESS) {
    ERROR("Failed to initialize all required components");
    return 1;
  }

  if (cloudcam_connect_blocking(&ctx) != SUCCESS) {
    ERROR("Failed to connect to AWSIoT service");
    return 2;
  }
  test_pub_thumb(ctx.iotc);

  while (1) {
    cloudcam_iot_poll_loop(&ctx);
  }

  cloudcam_global_cleanup();
  return 0;
}

// initialize a newly-allocated cloudcam context
// app_dir_path: root of application home dir, includes config dir
int cloudcam_init_ctx(cloudcam_ctx *ctx, char *app_dir_path) {
  bzero(ctx, sizeof(cloudcam_ctx));  // set to all 0s

  // copy app path
  ctx->app_dir_path = strdup(app_dir_path);

  // alloc IoT client
  AWS_IoT_Client *iotc = malloc(sizeof(AWS_IoT_Client));
  ctx->iotc = iotc;

  return SUCCESS;
}

// block until client is connected
IoT_Error_t cloudcam_connect_blocking(cloudcam_ctx *ctx) {
  assert(ctx->iotc);
  assert(ctx->app_dir_path);

  // attempt connect
  IoT_Error_t rc;
  do {
    rc = cloudcam_init_iot_client(ctx);
    if (rc != SUCCESS) {
      WARN("Failed to connect, will retry...")
      sleep(10);
    }
  } while (rc != SUCCESS);

  // should now be connected!

  // subscribe to topics and shadow deltas
  rc = cloudcam_iot_subscribe(ctx->iotc);
  if (rc != SUCCESS) {
    WARN("Failed to subscribe to topic");
    return rc;
  }

  return SUCCESS;
}

int cloudcam_free_ctx(cloudcam_ctx *ctx) {
  if (ctx->app_dir_path)
    free(ctx->app_dir_path);

  if (ctx->iotc) {
    // more cleanup goes here
    free(ctx->iotc);
  }

  return SUCCESS;
}

void cloudcam_init_logging() {
  openlog("cloudcam", LOG_PID | LOG_CONS, LOG_USER);

  tolog(&stdout);
  tolog(&stderr);
}

// main application setup
int cloudcam_global_init() {
  cloudcam_init_logging();
  CURLcode curl_ret = curl_global_init(CURL_GLOBAL_SSL);
  if (curl_ret) {
    ERROR("Failed to initialize libcurl %d", curl_ret);
    return curl_ret;
  }
  return SUCCESS;
}

void cloudcam_global_cleanup() {
  closelog();
}

void test_pub_thumb(AWS_IoT_Client *iotc) {
  IoT_Error_t rc;

  // // read image
  // struct stat file_stat;
  // int thumb_fh;
  // char sample_file[PATH_MAX + 1];
  // snprintf(sample_file, sizeof(sample_file), "%s/%s", dir, "sample/snowdino.jpg");
  // thumb_fh = open(sample_file, O_RDONLY);
  // if (thumb_fh == -1) {
  //   ERROR("Error opening %s: %s", sample_file, strerror(errno));
  //   return;
  // }
  // if (fstat(thumb_fh, &file_stat) != 0) {
  //   close(thumb_fh);
  //   ERROR("fstat error: %s", strerror(errno));
  //   return;
  // }
  // long long file_size = (long long)file_stat.st_size;
  // uint8_t *img = (uint8_t *)malloc(file_size);
  // ssize_t bytes_read = read(thumb_fh, img, file_size);
  // if (bytes_read < file_size) {
  //   ERROR("failed to read in image file");
  //   close(thumb_fh);
  //   return;
  // }
  // printf("read image bytes: %lld\n", (long long)bytes_read);
  // close(thumb_fh);

  struct timeval start,end;
  gettimeofday(&start, NULL);

  // when this is published, a message will be sent back on
  // cloudcam/thumb/request_snapshot

  // build message params
  IoT_Publish_Message_Params params;
  params.qos = QOS0;
  char *t = "{\"abc\":123}";
  params.payload = t;
  params.payloadLen = strlen(t) + 1;
  //  Msg.pPayload = (void *) img;
  //Msg.PayloadLen = 10;//bytes_read;

  // send message
  rc = aws_iot_mqtt_publish(iotc, CLOUDCAM_TOPIC_THUMBNAIL_TEST, strlen(CLOUDCAM_TOPIC_THUMBNAIL_TEST), &params);
  gettimeofday(&end, NULL);

  long long sec = (long long)(end.tv_sec - start.tv_sec);
  long long usec = (long long)(end.tv_usec - start.tv_usec);
  printf("dur, sec: %lld, usec: %lld\n", sec, usec);
  
  if (rc == SUCCESS) {
    INFO("published to %s", CLOUDCAM_TOPIC_THUMBNAIL_TEST);
  } else {
    ERROR("failed to publish to topic %s, rv=%d", CLOUDCAM_TOPIC_THUMBNAIL_TEST, rc);
  }
}


