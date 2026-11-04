CC ?= gcc
ARM_CC ?= arm-linux-gnueabihf-gcc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic
CPPFLAGS += -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L
LDLIBS += -pthread -lrt -lm
SOURCES = sms_app.c circular_buffer.c sensor_thread.c processor_thread.c \
	alert_thread.c logger_thread.c
HEADERS = sms_app.h ../driver/sms_sensor.h

.PHONY: all host arm clean
all: host

host: sms_app_x86

sms_app_x86: $(SOURCES) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SOURCES) $(LDLIBS)

arm: sms_app

sms_app: $(SOURCES) $(HEADERS)
	$(ARM_CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SOURCES) $(LDLIBS)

clean:
	rm -f sms_app sms_app_x86
