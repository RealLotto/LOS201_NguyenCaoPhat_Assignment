#!/bin/sh
set -eu

MODULE=/lib/modules/5.15.0/lab2_driver.ko
DEVICE=/dev/lab2
SYSFS=/sys/class/lab2_class/lab2

echo '=== LAB-02: procfs and sysfs Test ==='
if grep -q '^lab2_driver ' /proc/modules; then
    rmmod lab2_driver
fi
insmod "$MODULE"
if [ ! -c "$DEVICE" ]; then
    rm -f "$DEVICE"
    mknod -m 666 "$DEVICE" c 240 0
fi
chmod 666 "$DEVICE"

echo '[1] Overwrite counter test:'
printf first > "$DEVICE"
printf second > "$DEVICE"
printf third > "$DEVICE"
[ "$(cat "$SYSFS/overwrite_count")" = 2 ] || {
    echo 'ERROR: overwrite_count should be 2 after three writes from an empty buffer'
    exit 1
}

echo '[2] Seek test:'
printf ABCDEFGH > "$DEVICE"
actual="$(dd if="$DEVICE" bs=1 skip=3 count=2 2>/dev/null)"
[ "$actual" = DE ] || {
    echo "ERROR: seeking to offset 3 returned [$actual], expected [DE]"
    exit 1
}
actual="$(dd if="$DEVICE" bs=1 skip=20 count=1 2>/dev/null)"
[ -z "$actual" ] || {
    echo "ERROR: reading past the end returned [$actual]"
    exit 1
}

echo '[3] Reverse-read test:'
printf abc > "$DEVICE"
echo 1 > "$SYSFS/reverse_read"
[ "$(cat "$DEVICE")" = cba ] || {
    echo 'ERROR: reverse_read=1 did not return cba'
    exit 1
}
echo 0 > "$SYSFS/reverse_read"
[ "$(cat "$DEVICE")" = abc ] || {
    echo 'ERROR: reverse_read=0 did not return abc'
    exit 1
}
if echo 2 > "$SYSFS/reverse_read" 2>/dev/null; then
    echo 'ERROR: reverse_read accepted a value other than 0 or 1'
    exit 1
fi

echo '--- /proc/lab2_info (before write) ---'
cat /proc/lab2_info

expected='Embedded Linux Lab2 Test Data'
printf '%s\n' "$expected" > "$DEVICE"
echo '--- /proc/lab2_info (after write) ---'
cat /proc/lab2_info
grep -F "$expected" /proc/lab2_info >/dev/null

echo '--- sysfs attributes ---'
for attribute in buffer_len open_count reverse_read overwrite_count last_data; do
    [ -r "$SYSFS/$attribute" ] || {
        echo "ERROR: missing sysfs attribute $attribute"
        exit 1
    }
    printf '%s: ' "$attribute"
    cat "$SYSFS/$attribute"
done
[ "$(cat "$SYSFS/buffer_len")" -gt 0 ]
[ "$(cat "$SYSFS/reverse_read")" = 0 ]
grep -F "$expected" "$SYSFS/last_data" >/dev/null
grep -F 'Overwrites    :' /proc/lab2_info >/dev/null
echo '=== procfs/sysfs Test PASSED ==='
