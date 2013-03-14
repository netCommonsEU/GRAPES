ifndef BASE
BASE = ../../..
else
vpath %.c $(BASE)/src/$(notdir $(CURDIR))
endif
CFGDIR ?= ../..

NH_INCARNATION ?= udp
NET_HELPER = ../../net_helper-$(NH_INCARNATION)

UTILS_DIR = ../../Utils

DELEGATE_HELPERS = libs3_delegate_helper.so mysql_delegate_helper.so
DELEGATE_HELPERS_DEPS = ../../Utils/request_handler.o \
			../../Utils/fifo_queue.o \
			../../config.o \
			$(NET_HELPER).o

CFLAGS += -I$(UTILS_DIR)
CFLAGS += -I../../CloudSupport

delegate_helpers: clean-root $(DELEGATE_HELPERS)

libs3_delegate_helper.so: LDFLAGS += -pthread
ifeq ($(PLATFORM), darwin)
libs3_delegate_helper.so: LDFLAGS += -dynamiclib
else
libs3_delegate_helper.so: LDFLAGS += -shared
endif
libs3_delegate_helper.so: LDFLAGS += -ls3
libs3_delegate_helper.so: CFLAGS += -fPIC
libs3_delegate_helper.so: libs3_delegate_helper.o $(DELEGATE_HELPERS_DEPS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

mysql_delegate_helper.so: LDFLAGS += -pthread
ifeq ($(PLATFORM), darwin)
mysql_delegate_helper.so: LDFLAGS += -dynamiclib
else
mysql_delegate_helper.so: LDFLAGS += -shared
endif
mysql_delegate_helper.so: LDLIBS += `mysql_config --libs`
mysql_delegate_helper.so: CFLAGS += -fPIC
mysql_delegate_helper.so: CFLAGS += ` mysql_config --cflags`
mysql_delegate_helper.so: mysql_delegate_helper.o $(DELEGATE_HELPERS_DEPS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

.PHONY: clean-root
clean-root:
	$(MAKE) -C ../.. clean

clean::
	rm -f *.so

include $(BASE)/src/utils.mak

