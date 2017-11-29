AXIS_USABLE_LIBS = UCLIBC GLIBC
include $(AXIS_TOP_DIR)/tools/build/rules/common.mak

PROGS     = launcher capture-jpeg-snapshot

CFLAGS += -Wall -g -O2

all:	$(PROGS)

launcher: launcher.o
	$(CC) $(LDFLAGS) $^ $(LIBS) $(LDLIBS) -o $@

capture-jpeg-snapshot: capture-jpeg-snapshot.o
	$(CC) $(LDFLAGS) $^ $(LIBS) $(LDLIBS) -lcapture -o $@

clean:
	rm -f $(PROGS) *.o core
	rm -f *.tar

