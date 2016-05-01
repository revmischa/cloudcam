###### --- This application
SRCS = src/cloudcam.c src/log.c src/iot.c
OBJS = $(SRCS:.c=.o)
PROGS = cloudcam

INCLUDE_DIRS += -Iinclude

###### --- compile/linker flags
CFLAGS += -Wall -g
#CFLAGS += -Wall -g -O2  ---- -O2 breaks mbedTLS on ARM

VENDOR_DIR = vendor

###### --- aws_iot SDK + mbedTLS
# main src dirs (relative to AXIS_TOP_DIR - change to wherever your AWS SDK lives)
AWSIOT_DIR = $(VENDOR_DIR)/aws_iot_client
MBEDTLS_DIR = $(VENDOR_DIR)/mbedtls_lib
LIBCURL_PKG = curl-7.48.0
LIBCURL_DIR = $(VENDOR_DIR)/$(LIBCURL_PKG)

###### --- Axis ACAP app
AXIS_DIR = axis

# Logging level control
LOG_FLAGS += -DIOT_DEBUG  # enable this for debug logs
LOG_FLAGS += -DIOT_INFO
LOG_FLAGS += -DIOT_WARN
LOG_FLAGS += -DIOT_ERROR

# mbedtls
MBEDTLS_SRC_DIR = $(MBEDTLS_DIR)/library
MBEDTLS_SRC_FILES += $(wildcard $(MBEDTLS_SRC_DIR)/*.c)
TLS_INCLUDE_DIR = -I $(MBEDTLS_DIR)/include
# aws_iot client/platform/shadow
IOT_CLIENT_DIR = $(AWSIOT_DIR)/aws_iot_src
PLATFORM_DIR = $(IOT_CLIENT_DIR)/protocol/mqtt/aws_iot_embedded_client_wrapper/platform_linux/mbedtls
PLATFORM_COMMON_DIR = $(IOT_CLIENT_DIR)/protocol/mqtt/aws_iot_embedded_client_wrapper/platform_linux/common
SHADOW_SRC_DIR = $(IOT_CLIENT_DIR)/shadow
# IoT client directory
IOT_INCLUDE_DIRS += -I $(IOT_CLIENT_DIR)/protocol/mqtt
IOT_INCLUDE_DIRS += -I $(IOT_CLIENT_DIR)/protocol/mqtt/aws_iot_embedded_client_wrapper
IOT_INCLUDE_DIRS += -I $(IOT_CLIENT_DIR)/protocol/mqtt/aws_iot_embedded_client_wrapper/platform_linux
IOT_INCLUDE_DIRS += -I $(PLATFORM_COMMON_DIR)
IOT_INCLUDE_DIRS += -I $(PLATFORM_DIR)
IOT_INCLUDE_DIRS += -I $(SHADOW_SRC_DIR)
IOT_INCLUDE_DIRS += -I $(IOT_CLIENT_DIR)/utils
IOT_INCLUDE_DIRS += -I config
IOT_SRC_FILES += $(IOT_CLIENT_DIR)/protocol/mqtt/aws_iot_embedded_client_wrapper/aws_iot_mqtt_embedded_client_wrapper.c
IOT_SRC_FILES += $(IOT_CLIENT_DIR)/utils/jsmn.c
IOT_SRC_FILES += $(IOT_CLIENT_DIR)/utils/aws_iot_json_utils.c
IOT_SRC_FILES += $(wildcard $(PLATFORM_DIR)/*.c)
IOT_SRC_FILES += $(wildcard $(PLATFORM_COMMON_DIR)/*.c)
IOT_SRC_FILES += $(wildcard $(SHADOW_SRC_DIR)/*.c)
# MQTT Paho Embedded C client directory
MQTT_DIR = $(AWSIOT_DIR)/aws_mqtt_embedded_client_lib
MQTT_C_DIR = $(MQTT_DIR)/MQTTClient-C/src
MQTT_EMB_DIR = $(MQTT_DIR)/MQTTPacket/src
MQTT_INCLUDE_DIR += -I $(MQTT_EMB_DIR)
MQTT_INCLUDE_DIR += -I $(MQTT_C_DIR)
MQTT_SRC_FILES += $(wildcard $(MQTT_EMB_DIR)/*.c)
MQTT_SRC_FILES += $(MQTT_C_DIR)/MQTTClient.c
# aws_iot include dirs
INCLUDE_DIRS += $(IOT_INCLUDE_DIRS) 
INCLUDE_DIRS += $(MQTT_INCLUDE_DIR) 
INCLUDE_DIRS += $(TLS_INCLUDE_DIR)
INCLUDE_DIRS += $(APP_INCLUDE_DIRS)
# aws_iot src dirs
AWS_SRC_FILES += $(MQTT_SRC_FILES)
AWS_SRC_FILES += $(MBEDTLS_SRC_FILES)
AWS_SRC_FILES += $(IOT_SRC_FILES)
AWS_OBJS = $(AWS_SRC_FILES:.c=.o)
AWS_LIB = lib/libaws-iot.a

###### --- build flags
CFLAGS += $(LOG_FLAGS)
CFLAGS += $(INCLUDE_DIRS)
BUILD = $(CC) $(CFLAGS) $(LDFLAGS)

###### --- targets
.PHONY:	all aws-objs clean dist debug cloudcam host

cloudcam: dep $(AWS_LIB)
	$(BUILD) -o cloudcam $(SRCS) -Llib -laws-iot
all:	$(PROGS)

%.o: %.c
	@echo Compiling $<
	@$(BUILD) -c -o $@ $<

$(AWS_LIB): lib $(AWS_OBJS)
	ar rcs $(AWS_LIB) $(AWS_OBJS)

clean: clean-aws clean-prog clean-mbedtls clean-eap clean-target
clean-prog:
	rm -f $(PROGS) *.o core
clean-aws:
	rm -f $(AWS_OBJS) $(AWS_LIB)
clean-mbedtls:
	$(MAKE) -C $(MBEDTLS_DIR) clean

# Axis ACAP app project
clean-eap:
	$(MAKE) -C $(AXIS_DIR) clean-eap
clean-target:
	$(MAKE) -C $(AXIS_DIR) clean-target
dist:
	$(MAKE) -C $(AXIS_DIR) dist
upload:
	$(MAKE) -C $(AXIS_DIR) upload

# build for host system not axis cam
host: distclean cloudcam

lib:
	mkdir -p lib


### vendor depdencies
$(VENDOR_DIR):
	mkdir $(VENDOR_DIR)
dep: $(AWSIOT_DIR) $(MBEDTLS_DIR) $(LIBCURL_DIR)
$(AWSIOT_DIR): | $(VENDOR_DIR)
	git clone -b v1.1.2 --depth 1 https://github.com/aws/aws-iot-device-sdk-embedded-C.git $(AWSIOT_DIR)
$(MBEDTLS_DIR): | $(VENDOR_DIR)
	curl https://s3.amazonaws.com/aws-iot-device-sdk-embedded-c/linux_mqtt_mbedtls-1.1.0.tar > $(VENDOR_DIR)/linux_mqtt_mbedtls.tar
	cd $(VENDOR_DIR) && tar -xf linux_mqtt_mbedtls.tar mbedtls_lib
	rm $(VENDOR_DIR)/linux_mqtt_mbedtls.tar
libcurl: $(LIBCURL_DIR)
$(LIBCURL_DIR): $(VENDOR_DIR)/curl.tar.bz2 | $(VENDOR_DIR)
	cd $(VENDOR_DIR) && tar -xjf curl.tar.bz2
	rm $(VENDOR_DIR)/curl.tar.bz2
$(VENDOR_DIR)/curl.tar.bz2:
	curl https://curl.haxx.se/download/$(LIBCURL_PKG).tar.bz2 > $(VENDOR_DIR)/curl.tar.bz2

