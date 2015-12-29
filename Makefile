
TARGET ?= MyMiBand
CROSS_COMPILE ?=
EXTRA_CFLAGS  ?=
EXTRA_LDFLAGS ?=

RM := @rm -f
CC := ${CROSS_COMPILE}gcc

TARGET_CFLAGS  := -g -Wall -I. $(EXTRA_CFLAGS)
TARGET_LDFLAGS := $(EXTRA_LDFLAGS) -lpthread

OBJS    = $(addsuffix .o, $(notdir $(basename $(wildcard *.c))))

LIBRC_SOURCE   ?= ../LibRC
TARGET_CFLAGS  += -I$(LIBRC_SOURCE)/evhr
TARGET_LDFLAGS += -L$(LIBRC_SOURCE)/evhr -levhr
TARGET_CFLAGS  += -I$(LIBRC_SOURCE)/qlist
TARGET_LDFLAGS += -L$(LIBRC_SOURCE)/qlist -lqlist
TARGET_CFLAGS  += -I$(LIBRC_SOURCE)/eble
TARGET_LDFLAGS += -L$(LIBRC_SOURCE)/eble -leble

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

