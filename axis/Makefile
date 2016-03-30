###### --- This application
SRCS = cloudcam_axis.c
OBJS = $(SRCS:.c=.o)
PROGS = cloudcam

INCLUDE_DIRS += -Iinclude
CFLAGS += -DCLOUDCAM_TARGET_AXIS

###### --- Axis build settings
AXIS_USABLE_LIBS = UCLIBC GLIBC
NO_SUBDIR_RECURSION = 1
AXIS_OPT_DEBUG = 1
#include $(AXIS_TOP_DIR)/tools/build/Rules.axis
-include $(AXIS_TOP_DIR)/tools/build/rules/common.mak

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

AWS_LIB = lib/libaws-iot.a

###### --- build flags
CFLAGS += $(LOG_FLAGS)
CFLAGS += $(INCLUDE_DIRS)
BUILD = $(CC) $(CFLAGS) $(LDFLAGS)

###### --- targets
.PHONY:	all clean dist debug cloudcam host

cloudcam: $(AWS_LIB)
	$(BUILD) -o cloudcam $(SRCS) -L../lib -laws-iot -lcloudcam
all:	$(PROGS)

%.o: %.c
	@echo Compiling $<
	@$(BUILD) -c -o $@ $<

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

