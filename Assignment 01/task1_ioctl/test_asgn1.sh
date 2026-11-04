#!/bin/sh
set -eu

DEVICE=${DEVICE:-/dev/asgn1}
MODULE=./asgn1_driver.ko
MAJOR=242
HELPER=./asgn1_test
CREATED_NODE=0
LOADED_MODULE=0

cleanup() {
	if [ "$LOADED_MODULE" -eq 1 ]; then
		rmmod asgn1_driver || true
	fi
	if [ "$CREATED_NODE" -eq 1 ]; then
		rm -f "$DEVICE"
	fi
}
trap cleanup EXIT HUP INT TERM

test "$(id -u)" -eq 0 || { echo "Run this script as root." >&2; exit 1; }
make helper
insmod "$MODULE"
LOADED_MODULE=1
if [ ! -e "$DEVICE" ]; then
	mknod "$DEVICE" c "$MAJOR" 0
	CREATED_NODE=1
fi

"$HELPER" reset "$DEVICE"
printf 'hello' > "$DEVICE"
test "$(cat "$DEVICE")" = "hello"
"$HELPER" stats "$DEVICE"
"$HELPER" version "$DEVICE"
"$HELPER" badcmd "$DEVICE"

"$HELPER" mode 0 "$DEVICE"
printf 'abc' > "$DEVICE"
test "$(cat "$DEVICE")" = "[KERNEL]abc"
"$HELPER" mode 1 "$DEVICE"
printf 'xyz' > "$DEVICE"
test "$(cat "$DEVICE")" = "xyz"

printf 'reset-me' > "$DEVICE"
"$HELPER" reset "$DEVICE"
test -z "$(cat "$DEVICE")"
if [ "$LOADED_MODULE" -eq 1 ]; then
	rmmod asgn1_driver
	LOADED_MODULE=0
fi
echo "PASS: ASGN1 reset, stats, modes, version, and unknown ioctl tests."
dmesg | tail -n 40
