# our dep dirs
IOT_CLIENT_DIR = aws_iot_client
MBEDTLS_DIR = mbedtls_lib
LIBCURL_PKG = curl-7.48.0

# log level control
LOG_FLAGS += -DIOT_DEBUG  # enable this for debug logs
LOG_FLAGS += -DIOT_INFO
LOG_FLAGS += -DIOT_WARN
LOG_FLAGS += -DIOT_ERROR

PLATFORM_DIR = $(IOT_CLIENT_DIR)/platform/linux
PLATFORM_COMMON_DIR = $(PLATFORM_DIR)/common
PLATFORM_TLS_DIR = $(PLATFORM_DIR)/mbedtls
PLATFORM_THREAD_DIR = $(PLATFORM_DIR)/pthread

# iot client sources
IOT_SRC_FILES += $(wildcard $(IOT_CLIENT_DIR)/src/*.c)
IOT_SRC_FILES += $(wildcard $(IOT_CLIENT_DIR)/external_libs/jsmn/*.c)
IOT_SRC_FILES += $(wildcard $(PLATFORM_DIR)/*.c)
IOT_SRC_FILES += $(wildcard $(PLATFORM_COMMON_DIR)/*.c)
IOT_SRC_FILES += $(wildcard $(PLATFORM_TLS_DIR)/*.c)
IOT_SRC_FILES += $(wildcard $(PLATFORM_THREAD_DIR)/*.c)
AWS_OBJS = $(IOT_SRC_FILES:.c=.o)

# static iot client lib
AWS_LIB = libaws-iot.a

# mbedtls
TLS_LIB_DIR = $(MBEDTLS_DIR)/library
INCLUDE_DIRS += -I $(MBEDTLS_DIR)/include
IOT_EXTERNAL_LIBS += -L$(TLS_LIB_DIR)
TLS_LIB_FILES = $(TLS_LIB_DIR)/libmbedtls.a $(TLS_LIB_DIR)/libmbedcrypto.a $(TLS_LIB_DIR)/libmbedx509.a

INCLUDE_DIRS += -I $(IOT_CLIENT_DIR)/include
INCLUDE_DIRS += -I $(IOT_CLIENT_DIR)/external_libs/jsmn
INCLUDE_DIRS += -I $(PLATFORM_COMMON_DIR)
INCLUDE_DIRS += -I $(PLATFORM_DIR)
INCLUDE_DIRS += -I $(PLATFORM_TLS_DIR)
INCLUDE_DIRS += -I $(PLATFORM_THREAD_DIR)
INCLUDE_DIRS += -I ../config

CFLAGS = $(INCLUDE_DIRS) $(LOG_FLAGS)
BUILD = $(CC) $(CFLAGS)

BUILT_AWS_LIB = .built_aws_lib

prep: prep-deps
build: prep $(BUILT_AWS_LIB)

# building libs
$(TLS_LIB_FILES): $(MBEDTLS_DIR)
	$(MAKE) -j6 -C $(MBEDTLS_DIR)
$(BUILT_AWS_LIB): $(AWS_OBJS) | $(TLS_LIB_FILES)
	ar rcs $(AWS_LIB) $(AWS_OBJS)
	touch $(BUILT_AWS_LIB)

%.o: %.c
	@echo Compiling $<
	$(BUILD) -c -o $@ $<

# track dep fetching
IOT_CLIENT_CLONED = .cloned_iot_client
MBEDTLS_EXTRACTED = .extracted_mbedtls
LIBCURL_EXTRACTED = .extracted_libcurl
prep-deps: $(IOT_CLIENT_CLONED) $(MBEDTLS_EXTRACTED) $(LIBCURL_EXTRACTED)	

# dep fetching/extraction
LIBCURL_DIR = $(LIBCURL_PKG)
$(IOT_CLIENT_CLONED):
	git clone -b v2.1.0 --depth 1 https://github.com/aws/aws-iot-device-sdk-embedded-C.git $(IOT_CLIENT_DIR)
	touch $(IOT_CLIENT_CLONED)
$(MBEDTLS_EXTRACTED): | $(IOT_CLIENT_CLONED)
	curl https://s3.amazonaws.com/aws-iot-device-sdk-embedded-c/linux_mqtt_mbedtls-1.1.0.tar \
		> linux_mqtt_mbedtls.tar
	tar -xf linux_mqtt_mbedtls.tar $(MBEDTLS_DIR)
	rm linux_mqtt_mbedtls.tar
	touch $(MBEDTLS_EXTRACTED)
$(LIBCURL_EXTRACTED):
	curl http://poz.party/vendor/$(LIBCURL_PKG).tar.bz2 > curl.tar.bz2
	tar -xjf curl.tar.bz2
	rm curl.tar.bz2
	touch $(LIBCURL_EXTRACTED)

clean-aws:
	rm -f $(AWS_OBJS) $(AWS_LIB)
clean-mbedtls:
	$(MAKE) -C $(MBEDTLS_DIR) clean
clean:
	rm -rf $(IOT_CLIENT_CLONED) $(MBEDTLS_EXTRACTED) $(LIBCURL_EXTRACTED) \
		$(LIBCURL_PKG) $(IOT_CLIENT_DIR) $(MBEDTLS_DIR) \
		$(AWS_OBJS) $(AWS_LIB) $(BUILT_AWS_LIB)
