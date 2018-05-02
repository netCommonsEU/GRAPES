ifndef BASE
BASE = ../..
else
vpath %.c $(BASE)/src/$(notdir $(CURDIR))
endif
CFGDIR ?= ..

OBJS = chunkidms_ops.o chunkidss_encoding.o chunkidms_encoding.o chunkidss_ops.o  chunkidms_trading.o

all: libmultiset.a

include $(BASE)/src/utils.mak
