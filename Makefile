
TARGET ?= MyMiBand
CROSS_COMPILE ?=
EXTRA_CFLAGS  ?=
EXTRA_LDFLAGS ?=

RM := @rm -f
CC := ${CROSS_COMPILE}gcc

TARGET_CFLAGS  := -g -Wall -I. $(EXTRA_CFLAGS)
TARGET_LDFLAGS := $(EXTRA_LDFLAGS)

OBJS    = $(addsuffix .o, $(notdir $(basename $(wildcard *.c))))

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

