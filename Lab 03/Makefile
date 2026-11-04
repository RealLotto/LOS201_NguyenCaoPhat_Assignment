CC ?= gcc
ARM_CC ?= arm-linux-gnueabihf-gcc
ARM_FLAGS ?= -march=armv7-a -mfpu=neon-vfpv3 -mfloat-abi=hard
ARM_LDFLAGS ?= -static
CFLAGS_DEBUG ?= -g -O0 -Wall -Wextra
CFLAGS_PROFILE ?= -g -O1 -Wall -Wextra

APPS := apps/leaky_app apps/leaky_app_fixed apps/perf_target
ARM_APPS := apps/buggy_app apps/leaky_app_arm apps/perf_target_arm

.PHONY: all arm host clean

all: host arm

host: apps/leaky_app apps/leaky_app_fixed apps/perf_target

arm: $(ARM_APPS)

apps/buggy_app: apps/buggy_app.c
	$(ARM_CC) $(CFLAGS_DEBUG) $(ARM_FLAGS) $(ARM_LDFLAGS) -o $@ $<

apps/leaky_app: apps/leaky_app.c
	$(CC) $(CFLAGS_DEBUG) -o $@ $<

apps/leaky_app_fixed: apps/leaky_app.c
	$(CC) $(CFLAGS_DEBUG) -DLEAKY_APP_FIXED -o $@ $<

apps/perf_target: apps/perf_target.c
	$(CC) $(CFLAGS_PROFILE) -o $@ $< -lm

apps/buggy_app_arm: apps/buggy_app.c
	$(ARM_CC) $(CFLAGS_DEBUG) $(ARM_FLAGS) $(ARM_LDFLAGS) -o $@ $<

apps/leaky_app_arm: apps/leaky_app.c
	$(ARM_CC) $(CFLAGS_DEBUG) $(ARM_FLAGS) $(ARM_LDFLAGS) -o $@ $<

apps/perf_target_arm: apps/perf_target.c
	$(ARM_CC) $(CFLAGS_PROFILE) $(ARM_FLAGS) $(ARM_LDFLAGS) -o $@ $< -lm

clean:
	rm -f $(APPS) $(ARM_APPS) apps/buggy_app_arm
