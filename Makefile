###### --- This application
SRCS = src/cloudcam.c src/log.c src/iot.c
OBJS = $(SRCS:.c=.o)
PROGS = cloudcam

INCLUDE_DIRS += -Iinclude
CFLAGS += -DCLOUDCAM_TARGET_AXIS

###### --- Axis build settings
AXIS_USABLE_LIBS = UCLIBC GLIBC
NO_SUBDIR_RECURSION = 1
AXIS_OPT_DEBUG = 1
#include $(AXIS_TOP_DIR)/tools/build/Rules.axis
include $(AXIS_TOP_DIR)/tools/build/rules/common.mak

###### --- Host/Cross-compile defines
ifeq ($(AXIS_BUILDTYPE),host)
  CFLAGS += -DCLOUDCAM_LOG_DUPOUT
else ifeq ($(AXIS_BUILDTYPE),armv6-axis-linux-gnueabi)
  #CFLAGS += -DREVERSED # for armv6
endif # AXIS_BUILDTYPE

###### --- compile/linker flags
CFLAGS += -Wall -g
#CFLAGS += -Wall -g -O2  ---- -O2 breaks mbedTLS on ARM
LDFLAGS += -L$(PWD)
# axis ACAP libs
# ...

###### --- aws_iot SDK + mbedTLS
# main src dirs (relative to AXIS_TOP_DIR - change to wherever your AWS SDK lives)
AWSIOT_DIR = aws_iot_client
MBEDTLS_DIR = mbedtls_lib

# Logging level control
#LOG_FLAGS += -DIOT_DEBUG
LOG_FLAGS += -DIOT_INFO
LOG_FLAGS += -DIOT_WARN
LOG_FLAGS += -DIOT_ERROR

# mbedtls
MBEDTLS_SRC_DIR = $(MBEDTLS_DIR)/library
MBEDTLS_SRC_FILES += $(shell find $(MBEDTLS_SRC_DIR)/ -name '*.c')
TLS_INCLUDE_DIR = -I $(MBEDTLS_DIR)/include
# aws_iot client/platform/shadow
IOT_CLIENT_DIR=$(AWSIOT_DIR)/aws_iot_src
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
IOT_SRC_FILES += $(shell find $(PLATFORM_DIR)/ -name '*.c')
IOT_SRC_FILES += $(shell find $(PLATFORM_COMMON_DIR)/ -name '*.c')
IOT_SRC_FILES += $(shell find $(SHADOW_SRC_DIR)/ -name '*.c')
# MQTT Paho Embedded C client directory
MQTT_DIR = $(AWSIOT_DIR)/aws_mqtt_embedded_client_lib
MQTT_C_DIR = $(MQTT_DIR)/MQTTClient-C/src
MQTT_EMB_DIR = $(MQTT_DIR)/MQTTPacket/src
MQTT_INCLUDE_DIR += -I $(MQTT_EMB_DIR)
MQTT_INCLUDE_DIR += -I $(MQTT_C_DIR)
MQTT_SRC_FILES += $(shell find $(MQTT_EMB_DIR)/ -name '*.c')
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

cloudcam: $(AWS_LIB)
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
clean-eap:
	rm -f *.tar
	rm -f *.eap *.eap.old package.conf.orig
clean-target:
	rm -f .target-makefrag

dist:
	create-package.sh armv6

upload: dist
	eap-install.sh install
	eap-install.sh start

# build for host system not axis cam
host: distclean cloudcam

lib:
	mkdir -p lib
