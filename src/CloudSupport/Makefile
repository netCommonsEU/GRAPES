ifndef BASE
BASE = ../..
else
vpath %.c $(BASE)/src/$(notdir $(CURDIR))
endif
CFGDIR ?= ..

UTILS_DIR = ../Utils

OBJS = cloud_helper.o
#ifneq ($(ARCH),win32)
#  OBJS += cloud_helper_utils.o
#endif

CFLAGS += -I$(UTILS_DIR)

ifdef DELEGATE
OBJS += cloud_helper_delegate.o
CPPFLAGS += -DDELEGATE
endif

all: $(OBJS)

include $(BASE)/src/utils.mak

