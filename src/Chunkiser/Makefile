ifndef BASE
BASE = ../..
else
vpath %.c $(BASE)/src/$(notdir $(CURDIR))
endif
CFGDIR ?= ..

OBJS = input-stream.o           \
       input-stream-dummy.o     \
       output-stream.o          \
       output-stream-dummy.o

ifneq ($(ARCH),win32)
OBJS += \
       input-stream-dumb.o      \
       input-stream-ts.o        \
       input-stream-udp.o       \
       output-stream-raw.o      \
       output-stream-rtp.o      \
       output-stream-udp.o
endif

ifdef FFDIR
OBJS += input-stream-avf.o output-stream-avf.o
OBJS += input-stream-avf.o input-stream-ipb.o output-stream-avf.o chunkiser_attrib.o
ifdef GTK
OBJS += output-stream-play.o
endif
endif

ifeq ($(strip $(PJDIR)),)
OBJS += rtp_rtcp.o
OBJS += input-stream-rtp.o
else
OBJS += input-stream-rtp.o
endif

all: libchunkiser.a

include $(BASE)/src/utils.mak
ifdef FFDIR
output-stream-avf.o: CPPFLAGS += -I$(FFDIR)
input-stream-avf.o: CPPFLAGS += -I$(FFDIR)
input-stream-ipb.o: CPPFLAGS += -I$(FFDIR)
output-stream-play.o: CPPFLAGS += -I$(FFDIR)
CPPFLAGS += -DAVF
ifdef GTK
output-stream-play.o: CPPFLAGS += `pkg-config --cflags gtk+-2.0`
CPPFLAGS += -DGTK
endif
endif

ifeq ($(strip $(PJDIR)),)
CPPFLAGS += -DRTP
else
input-stream-rtp.o: CFLAGS += -I$(PJDIR)/include
CPPFLAGS += -DRTP -DPJLIB_RTP
endif
