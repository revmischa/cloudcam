#ifndef CLOUDCAM_IOT_H
#define CLOUDCAM_IOT_H

#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"
#include "aws_iot_config.h"
#include "aws_iot_shadow_interface.h"

extern const char *CLOUDCAM_TOPIC_THUMBNAIL_TEST;

void shadow_delta_handler(const char *json_str, uint32_t json_str_len, jsonStruct_t *js);
void thumbnail_requested_handler(AWS_IoT_Client *iotc, char *topic_name, unsigned short topic_name_len, IoT_Publish_Message_Params *params, void *data);
void cloudcam_iot_disconnect_handler(AWS_IoT_Client *, void *);

IoT_Error_t cloudcam_init_iot_client(AWS_IoT_Client *iotc, char *app_path);
void cloudcam_iot_poll_loop(AWS_IoT_Client *iotc);
void cloudcam_iot_subscribe(AWS_IoT_Client *iotc);
IoT_Error_t cloudcam_subscribe_topic(AWS_IoT_Client *iotc, char *topic, pApplicationHandler_t handler);

//  int MQTTcallbackHandler(MQTTCallbackParams params);

#endif // CLOUDCAM_IOT_H
