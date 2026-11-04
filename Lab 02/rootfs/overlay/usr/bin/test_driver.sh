#!/bin/sh
set -eu

MODULE=/lib/modules/5.15.0/lab2_driver.ko
DEVICE=/dev/lab2

echo '=== LAB-02: Device Driver Test ==='
if ! grep -q '^lab2_driver ' /proc/modules; then
    echo '[1] Loading lab2_driver...'
    insmod "$MODULE"
else
    echo '[1] lab2_driver was loaded automatically by rcS'
fi

echo '[2] Loaded module:'
grep '^lab2_driver ' /proc/modules

if [ ! -c "$DEVICE" ]; then
    rm -f "$DEVICE"
    mknod -m 666 "$DEVICE" c 240 0
fi
chmod 666 "$DEVICE"
ls -l "$DEVICE"

echo '[3] Write/read test:'
expected='Hello from userspace, LAB-02!'
printf '%s\n' "$expected" > "$DEVICE"
actual="$(cat "$DEVICE")"
echo "  Read back: [$actual]"
[ "$actual" = "$expected" ] || {
    echo 'ERROR: read data does not match the written message'
    exit 1
}

echo '[4] Multiple write/read test:'
for i in 1 2 3; do
    expected="Message_$i"
    printf '%s\n' "$expected" > "$DEVICE"
    actual="$(cat "$DEVICE")"
    echo "  $actual"
    [ "$actual" = "$expected" ] || {
        echo "ERROR: iteration $i did not round-trip"
        exit 1
    }
done

echo '[5] Recent driver messages:'
dmesg | grep 'lab2_driver:' | tail -10 || true
echo '=== Test PASSED ==='
