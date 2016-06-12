#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <limits.h>
#include <fcntl.h>

// override logging with axis syslog logz
#include "cloudcam/log.h"
// AWS IoT interfacing
#include "cloudcam/iot.h"

static void test_pub_thumb(AWS_IoT_Client *iotc);
static void cloudcam_cleanup(AWS_IoT_Client *iotc);
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
  cloudcam_init_logging();

  AWS_IoT_Client iotc;
  cloudcam_init_iot_client(&iotc, argv[0]);
  
  // subscribe to topics and shadow deltas
  cloudcam_iot_subscribe(&iotc);

  test_pub_thumb(&iotc);
  cloudcam_iot_poll_loop(&iotc);

  cloudcam_cleanup(&iotc);
  return 0;
}

void cloudcam_init_logging() {
  openlog("cloudcam", LOG_PID | LOG_CONS, LOG_USER);

  tolog(&stdout);
  tolog(&stderr);
}

void cloudcam_cleanup(AWS_IoT_Client *iotc) {
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


