AXIS_USABLE_LIBS = UCLIBC GLIBC
include $(AXIS_TOP_DIR)/tools/build/rules/common.mak

AWSIOT_DIR = $(AXIS_TOP_DIR)/linux_mqtt_mbedtls-1.0.1
MBEDTLS_DIR = $(AWSIOT_DIR)/mbedtls_lib

SRCS = cloudcam.c
OBJS = $(SRCS:.c=.o)

PROGS = cloudcam

CFLAGS += -Wall -g -O2
ifeq ($(AXIS_BUILDTYPE),host)
LDFLAGS += -lcapturehost -ljpeg
else
LDFLAGS += -lcapture
endif # AXIS_BUILDTYPE == host

LDFLAGS += -L$(MBEDTLS_DIR)/library -lmbedtls

mbedtls:
	$(MAKE) -C $(MBEDTLS_DIR)
aws-iot: mbedtls
#	$(MAKE) -C  
all:	aws-iot $(PROGS)

$(PROGS): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) $(LDLIBS) -o $@

clean:
	$(MAKE) -C $(MBEDTLS_DIR) clean
	rm -f $(PROGS) *.o core
	rm -f *.tar

