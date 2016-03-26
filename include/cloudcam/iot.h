#ifndef CLOUDCAM_IOT_H
#define CLOUDCAM_IOT_H

#include "aws_iot_version.h"
#include "aws_iot_mqtt_interface.h"
#include "aws_iot_config.h"
#include "aws_iot_shadow_interface.h"

void cloudcam_iot_poll_loop(MQTTClient_t *mc);
int thumbnail_requested_handler(MQTTCallbackParams params);
void cloudcam_iot_subscribe();
IoT_Error_t cloudcam_subscribe_topic(char *topic, iot_message_handler handler);
void disconnectCallbackHandler(void);

//  int MQTTcallbackHandler(MQTTCallbackParams params);

#endif // CLOUDCAM_IOT_H
