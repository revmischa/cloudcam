###### --- This application
SRCS = src/cloudcam.c src/log.c src/iot.c
OBJS = $(SRCS:.c=.o)
PROGS = cloudcam

INCLUDE_DIRS += -I include -I config

###### --- compile/linker flags
CFLAGS += -Wall -g
#CFLAGS += -Wall -g -O2  ---- -O2 breaks mbedTLS on ARM

VENDOR_DIR = vendor

###### --- aws_iot SDK + mbedTLS
# main src dirs (relative to AXIS_TOP_DIR - change to wherever your AWS SDK lives)
IOT_CLIENT_DIR = $(VENDOR_DIR)/aws_iot_client
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

PLATFORM_DIR = $(IOT_CLIENT_DIR)/platform/linux
PLATFORM_COMMON_DIR = $(PLATFORM_DIR)/common
PLATFORM_TLS_DIR = $(PLATFORM_DIR)/mbedtls
PLATFORM_THREAD_DIR = $(PLATFORM_DIR)/pthread

INCLUDE_DIRS += -I $(IOT_CLIENT_DIR)/include
INCLUDE_DIRS += -I $(IOT_CLIENT_DIR)/external_libs/jsmn
INCLUDE_DIRS += -I $(PLATFORM_COMMON_DIR)
INCLUDE_DIRS += -I $(PLATFORM_DIR)
INCLUDE_DIRS += -I $(PLATFORM_TLS_DIR)
INCLUDE_DIRS += -I $(PLATFORM_THREAD_DIR)

IOT_SRC_FILES += $(wildcard $(IOT_CLIENT_DIR)/src/*.c)
IOT_SRC_FILES += $(wildcard $(IOT_CLIENT_DIR)/external_libs/jsmn/*.c)
IOT_SRC_FILES += $(wildcard $(PLATFORM_DIR)/*.c)
IOT_SRC_FILES += $(wildcard $(PLATFORM_COMMON_DIR)/*.c)
IOT_SRC_FILES += $(wildcard $(PLATFORM_TLS_DIR)/*.c)
IOT_SRC_FILES += $(wildcard $(PLATFORM_THREAD_DIR)/*.c)

# TLS - mbedtls
# MBEDTLS_DIR = $(IOT_CLIENT_DIR)/external_libs/mbedTLS
TLS_LIB_DIR = $(MBEDTLS_DIR)/library
INCLUDE_DIRS += -I $(MBEDTLS_DIR)/include
IOT_EXTERNAL_LIBS += -L$(TLS_LIB_DIR)
TLS_LIB_FILES = $(TLS_LIB_DIR)/libmbedtls.a $(TLS_LIB_DIR)/libmbedcrypto.a $(TLS_LIB_DIR)/libmbedx509.a
# LDFLAGS += -Wl,-rpath,$(TLS_LIB_DIR)
LDFLAGS += $(TLS_LIB_FILES) -lpthread
LDFLAGS += $(IOT_EXTERNAL_LIBS)

AWS_SRC_FILES += $(APP_SRC_FILES)
AWS_SRC_FILES += $(IOT_SRC_FILES)

AWS_OBJS = $(AWS_SRC_FILES:.c=.o)
AWS_LIB = lib/libaws-iot.a

###### --- build flags
CFLAGS += $(LOG_FLAGS)
CFLAGS += $(INCLUDE_DIRS)
BUILD = $(CC) $(CFLAGS)

###### --- targets
.PHONY:	all aws-objs clean dist debug cloudcam host

cloudcam: dep $(AWS_LIB)
	$(BUILD) -o cloudcam $(SRCS) $(LDFLAGS) -Llib -laws-iot
all:	$(PROGS)

%.o: %.c
	@echo Compiling $<
	$(BUILD) -c -o $@ $<

$(TLS_LIB_FILES): $(MBEDTLS_DIR)
	$(MAKE) -C $(MBEDTLS_DIR)
$(AWS_LIB): $(AWS_OBJS) | lib $(TLS_LIB_FILES)
	ar rcs $(AWS_LIB) $(AWS_OBJS) $(IOT_INCLUDE_ALL_DIRS)

clean: clean-aws clean-prog clean-mbedtls clean-eap clean-target
clean-prog:
	rm -f $(PROGS) *.o core
clean-aws:
	rm -f $(AWS_OBJS) $(AWS_LIB)
clean-mbedtls:
	$(MAKE) -C $(MBEDTLS_DIR) clean
distclean: clean
	rm -rf $(VENDOR_DIR)

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
dep: $(IOT_CLIENT_DIR) $(MBEDTLS_DIR) $(LIBCURL_DIR)
$(IOT_CLIENT_DIR): | $(VENDOR_DIR)
	git clone -b v2.1.0 --depth 1 https://github.com/aws/aws-iot-device-sdk-embedded-C.git $(IOT_CLIENT_DIR)
$(MBEDTLS_DIR): | $(VENDOR_DIR) $(IOT_CLIENT_DIR)
	curl https://s3.amazonaws.com/aws-iot-device-sdk-embedded-c/linux_mqtt_mbedtls-1.1.0.tar \
		> $(VENDOR_DIR)/linux_mqtt_mbedtls.tar
	cd $(VENDOR_DIR) && tar -xf linux_mqtt_mbedtls.tar mbedtls_lib
	rm $(VENDOR_DIR)/linux_mqtt_mbedtls.tar
	# cp -r $(MBEDTLS_DIR)/* 
libcurl: $(LIBCURL_DIR)
$(LIBCURL_DIR): $(VENDOR_DIR)
	curl http://poz.party/vendor/$(LIBCURL_PKG).tar.bz2 > $(VENDOR_DIR)/curl.tar.bz2
	cd $(VENDOR_DIR) && tar -xjf curl.tar.bz2
	rm $(VENDOR_DIR)/curl.tar.bz2
	
