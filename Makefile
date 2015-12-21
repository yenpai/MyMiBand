
TARGET ?= MyMiBand
CROSS_COMPILE ?=
EXTRA_CFLAGS  ?=
EXTRA_LDFLAGS ?=

RM := @rm -f
CC := ${CROSS_COMPILE}gcc

TARGET_CFLAGS  := -g -Wall -I. $(EXTRA_CFLAGS)
TARGET_LDFLAGS := $(EXTRA_LDFLAGS)

OBJS    = $(addsuffix .o, $(notdir $(basename $(wildcard *.c))))

EVHR_SOURCE    ?= ../MyEventHandler
TARGET_CFLAGS  += -I$(EVHR_SOURCE)
TARGET_LDFLAGS += -L$(EVHR_SOURCE) -levhr

#BLUEZ_SOURCE   ?= ../bluez-5.35
#TARGET_CFLAGS  += -I$(BLUEZ_SOURCE)
TARGET_LDFLAGS += -lbluetooth

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "linking $@ ..."
	$(CC) $(OBJS) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) -o $@

%.o: %.c
	$(CC) $(TARGET_CFLAGS) -c -o $@ $<

clean:
	$(RM) $(TARGET)
	$(RM) $(OBJS)

