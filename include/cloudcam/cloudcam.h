#ifndef CLOUDCAM_H
#define CLOUDCAM_H

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
#include <curl/curl.h>
#include "aws_iot_mqtt_client_interface.h"
#include "cloudcam/ccgst.h"

// represents a cloudcam client context
typedef struct {
    char *app_dir_path;
    char *thing_name;
    char *commands_topic_name;
    char *client_id;
    char *endpoint;
    char *ca_path;
    char *client_crt_path;
    char *client_key_path;
    AWS_IoT_Client *iotc;
    ccgst_thread_ctx *gst;
} cloudcam_ctx;

// public functions
IoT_Error_t cloudcam_init_ctx(cloudcam_ctx *ctx, char *app_dir_path);
IoT_Error_t cloudcam_free_ctx(cloudcam_ctx *ctx);
IoT_Error_t cloudcam_connect_blocking(cloudcam_ctx *ctx);

#endif  // CLOUDCAM_H
