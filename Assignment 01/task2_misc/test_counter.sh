#!/bin/sh
set -eu

MODULE=./counter_driver.ko
LOADED_MODULE=0

cleanup() {
	if [ "$LOADED_MODULE" -eq 1 ]; then
		rmmod counter_driver || true
	fi
}
trap cleanup EXIT HUP INT TERM

test "$(id -u)" -eq 0 || { echo "Run this script as root." >&2; exit 1; }
insmod "$MODULE"
LOADED_MODULE=1
test -e /dev/counter
test -r /proc/counter_info

echo "Initial proc stats:"
cat /proc/counter_info
test "$(cat /dev/counter)" = "0"
echo "Before set-to-100:"
cat /proc/counter_info
printf '100\n' > /dev/counter
echo "After set-to-100:"
cat /proc/counter_info
test "$(cat /dev/counter)" = "100"
printf '0\n' > /dev/counter
echo "After reset-to-zero:"
cat /proc/counter_info
test "$(cat /dev/counter)" = "0"
printf '%s\n' '-1' > /dev/counter && { echo "negative value unexpectedly accepted" >&2; exit 1; } || :
printf '%s\n' 'not-a-number' > /dev/counter && { echo "invalid text unexpectedly accepted" >&2; exit 1; } || :

echo "Final proc stats:"
cat /proc/counter_info
rmmod counter_driver
LOADED_MODULE=0
dmesg | tail -n 40
echo "PASS: misc registration, read/increment, write/reset, validation, and proc stats."
