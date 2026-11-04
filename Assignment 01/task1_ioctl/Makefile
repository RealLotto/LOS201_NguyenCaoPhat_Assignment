obj-m += asgn1_driver.o

KDIR ?= /lib/modules/$(shell uname -r)/build
ARCH ?= arm
CROSS_COMPILE ?= arm-linux-gnueabihf-

.PHONY: all clean helper
all:
	$(MAKE) -C $(KDIR) M="$(CURDIR)" ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

helper:
	$(CC) -std=c11 -Wall -Wextra -O2 -o asgn1_test asgn1_test.c

clean:
	$(MAKE) -C $(KDIR) M="$(CURDIR)" ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
	rm -f asgn1_test
