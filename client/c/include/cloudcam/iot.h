#ifndef CLOUDCAM_IOT_H
#define CLOUDCAM_IOT_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <libgen.h>
#include <jansson.h>

#include "cloudcam.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"
#include "aws_iot_config.h"
#include "aws_iot_shadow_interface.h"

extern const char *CLOUDCAM_TOPIC_THUMBNAIL_TEST;

// callback handlers
void cloudcam_iot_command_handler(AWS_IoT_Client *pClient, char *pTopicName, uint16_t topicNameLen, IoT_Publish_Message_Params *pParams, void *pClientData);
void shadow_streams_delta_handler(const char *json_str, uint32_t json_str_len, jsonStruct_t *js);
void cloudcam_iot_disconnect_handler(AWS_IoT_Client *, void *);

// setup funcs
IoT_Error_t cloudcam_init_iot_client(cloudcam_ctx *ctx);
IoT_Error_t cloudcam_iot_connect(cloudcam_ctx *ctx);
IoT_Error_t cloudcam_iot_poll_loop(cloudcam_ctx *ctx);
IoT_Error_t cloudcam_iot_subscribe(cloudcam_ctx *ctx);

#endif // CLOUDCAM_IOT_H
