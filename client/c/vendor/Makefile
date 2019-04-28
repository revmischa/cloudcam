JANSSON_VERSION = 2.9

# our dep dirs
IOT_CLIENT_DIR = aws_iot_client
MBEDTLS_DIR = mbedtls_lib
LIBCURL_PKG = curl-7.61.0
JANSSON_DIR = jansson-$(JANSSON_VERSION)


# iot client sources
IOT_SRC_FILES += $(wildcard $(IOT_CLIENT_DIR)/src/*.c)
IOT_SRC_FILES += $(wildcard $(IOT_CLIENT_DIR)/external_libs/jsmn/*.c)
IOT_SRC_FILES += $(wildcard $(PLATFORM_DIR)/*.c)
IOT_SRC_FILES += $(wildcard $(PLATFORM_COMMON_DIR)/*.c)
IOT_SRC_FILES += $(wildcard $(PLATFORM_TLS_DIR)/*.c)
IOT_SRC_FILES += $(wildcard $(PLATFORM_THREAD_DIR)/*.c)
AWS_OBJS = $(IOT_SRC_FILES:.c=.o)

PLATFORM_DIR = $(IOT_CLIENT_DIR)/platform/linux
PLATFORM_COMMON_DIR = $(PLATFORM_DIR)/common
PLATFORM_TLS_DIR = $(PLATFORM_DIR)/mbedtls
PLATFORM_THREAD_DIR = $(PLATFORM_DIR)/pthread

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

CFLAGS = $(INCLUDE_DIRS)
BUILD = $(CC) $(CFLAGS)

BUILT_AWS_LIB = .built_aws_lib
BUILT_LIBCURL = .built_libcurl
BUILT_JANSSON = .built_jansson
LIBCURL_DIR = $(LIBCURL_PKG)

build: $(BUILT_AWS_LIB) $(BUILT_LIBCURL) $(BUILT_JANSSON) | prep
prep: prep-deps

# track dep fetching
MBEDTLS_EXTRACTED = .extracted_mbedtls
LIBCURL_EXTRACTED = .extracted_libcurl
JANSSON_EXTRACTED = .extracted_jansson$(JANSSON_VERSION)
prep-deps: $(IOT_CLIENT_DIR) $(MBEDTLS_EXTRACTED) $(LIBCURL_EXTRACTED) $(JANSSON_EXTRACTED)

# building libs
# make mbedtls
$(TLS_LIB_FILES): $(MBEDTLS_DIR)
	$(MAKE) -j6 -C $(MBEDTLS_DIR)
	$(MAKE) -C $(MBEDTLS_DIR) install DESTDIR=$(PWD)/vendor/${MBEDTLS_DIR}/build
# make aws lib
$(BUILT_AWS_LIB): $(AWS_OBJS) | $(TLS_LIB_FILES)
	ar rcs $(AWS_LIB) $(AWS_OBJS)
	touch $(BUILT_AWS_LIB)
# make libcurl
$(BUILT_LIBCURL): $(TLS_LIB_FILES)
	cd $(LIBCURL_DIR) && ./configure --disable-shared --without-ssl --without-librtmp --disable-ldap --with-mbedtls=$(PWD)/vendor/${MBEDTLS_DIR}/build --prefix=$(PWD)/vendor/$(LIBCURL_DIR)/build
	make -C $(LIBCURL_DIR) -j3
	make -C $(LIBCURL_DIR) install
	touch $(BUILT_LIBCURL)
$(BUILT_JANSSON): $(JANSSON_EXTRACTED)
	cd $(JANSSON_DIR) && ./configure --prefix=$(PWD)/vendor/$(JANSSON_DIR)/build
	make -C $(JANSSON_DIR) -j3
	make -C $(JANSSON_DIR) install
	touch $(BUILT_JANSSON)

%.o: %.c
	@echo Compiling $<
	$(BUILD) -c -o $@ $<

# dep fetching/extraction
$(IOT_CLIENT_DIR):
	git clone -b v2.1.1 --depth 1 https://github.com/aws/aws-iot-device-sdk-embedded-C.git $(IOT_CLIENT_DIR)
$(JANSSON_EXTRACTED):
	curl -L http://www.digip.org/jansson/releases/jansson-2.9.tar.gz > jansson.tar.gz
	tar -xzf jansson.tar.gz
	rm jansson.tar.gz
	touch $(JANSSON_EXTRACTED)
$(MBEDTLS_EXTRACTED): | $(IOT_CLIENT_CLONED)
	curl https://s3.amazonaws.com/aws-iot-device-sdk-embedded-c/linux_mqtt_mbedtls-1.1.0.tar \
		> linux_mqtt_mbedtls.tar
	tar -xf linux_mqtt_mbedtls.tar $(MBEDTLS_DIR)
	rm linux_mqtt_mbedtls.tar
	touch $(MBEDTLS_EXTRACTED)
$(LIBCURL_EXTRACTED):
	# curl http://poz.party/vendor/$(LIBCURL_PKG).tar.bz2 > curl.tar.bz2
	curl https://curl.haxx.se/download/$(LIBCURL_PKG).tar.bz2 > curl.tar.bz2
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
