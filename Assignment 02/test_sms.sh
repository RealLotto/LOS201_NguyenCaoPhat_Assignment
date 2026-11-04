#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DRIVER="$ROOT/driver/sms_sensor_driver.ko"
APP="$ROOT/app/sms_app"
LOG_DIR="$ROOT/logs"
APP_PID=
MODULE_LOADED=0
MQUEUE_MOUNTED=0

cleanup() {
	if [ -n "$APP_PID" ]; then
		kill -INT "$APP_PID" 2>/dev/null || true
		wait "$APP_PID" 2>/dev/null || true
	fi
	if [ "$MODULE_LOADED" -eq 1 ]; then
		rmmod sms_sensor_driver || true
	fi
	if [ "$MQUEUE_MOUNTED" -eq 1 ]; then
		umount /dev/mqueue || true
	fi
}
trap cleanup EXIT HUP INT TERM

test "$(id -u)" -eq 0 || { echo "Run this script as root in QEMU." >&2; exit 1; }
test -r "$DRIVER" || { echo "Build the ARM module before running this test." >&2; exit 1; }
test -x "$APP" || { echo "Build app/sms_app for ARM before running this test." >&2; exit 1; }
mkdir -p "$LOG_DIR"
mkdir -p /dev/mqueue
if ! grep -q ' /dev/mqueue mqueue ' /proc/mounts; then
	mount -t mqueue none /dev/mqueue
	MQUEUE_MOUNTED=1
fi
insmod "$DRIVER"
MODULE_LOADED=1
cat /proc/sms_stats > "$LOG_DIR/proc_sms_stats_before.txt"
LOG_START_LINES=0
if [ -f "$LOG_DIR/sms.log" ]; then
	LOG_START_LINES=$(wc -l < "$LOG_DIR/sms.log")
	if [ "$(wc -c < "$LOG_DIR/sms.log")" -gt 1048576 ]; then
		LOG_START_LINES=0
	fi
fi

"$APP" -i 500 -t 40 -h 80 -d 0 -s fifo -v \
	-L "$LOG_DIR/sms.log" -J "$LOG_DIR/jitter_report.txt" \
	> "$LOG_DIR/sms_sample_run.txt" 2>&1 &
APP_PID=$!
sleep 5
cat "/proc/$APP_PID/status" > "$LOG_DIR/memory_t5.txt"
cat /proc/sms_stats > "$LOG_DIR/proc_sms_stats_t5.txt"
sleep 25
cat "/proc/$APP_PID/status" > "$LOG_DIR/memory_t30.txt"
cat "/proc/$APP_PID/maps" > "$LOG_DIR/maps_t30.txt"
cat /proc/sms_stats > "$LOG_DIR/proc_sms_stats_t30.txt"
sleep 25
cat "/proc/$APP_PID/status" > "$LOG_DIR/memory_t55.txt"
cat /proc/sms_stats > "$LOG_DIR/proc_sms_stats_t55.txt"
sleep 5
kill -INT "$APP_PID"
wait "$APP_PID"
APP_PID=
cat /proc/sms_stats > "$LOG_DIR/proc_sms_stats_after.txt"
READ_COUNT=$(sed -n 's/^read_count: //p' "$LOG_DIR/proc_sms_stats_after.txt")
if [ "$READ_COUNT" -lt 110 ] || [ "$READ_COUNT" -gt 130 ]; then
	echo "Unexpected read_count after the 60-second run: $READ_COUNT" >&2
	exit 1
fi
for thread in sensor processor alert logger; do
	if ! tail -n "+$((LOG_START_LINES + 1))" "$LOG_DIR/sms.log" |
		grep -q "THREAD $thread started"; then
		echo "Missing logger entry for the $thread thread." >&2
		exit 1
	fi
done
rmmod sms_sensor_driver
MODULE_LOADED=0
dmesg > "$LOG_DIR/dmesg_sms.txt"
echo "PASS: SMS driver, proc stats, app threads, and SIGINT shutdown."
echo "Review collected evidence in: $LOG_DIR"
