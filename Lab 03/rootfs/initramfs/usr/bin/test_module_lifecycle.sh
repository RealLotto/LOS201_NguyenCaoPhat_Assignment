#!/bin/sh
set -eu

MODULE=/lib/modules/5.15.0/lab2_driver.ko
echo '=== LAB-02: Module lifecycle test ==='
grep -q '^lab2_driver ' /proc/modules
rmmod lab2_driver
if grep -q '^lab2_driver ' /proc/modules; then
    echo 'ERROR: module remained loaded after rmmod'
    exit 1
fi
[ ! -e /proc/lab2_info ]
[ ! -e /sys/class/lab2_class/lab2 ]
echo '[1] rmmod removed the module, proc entry, and sysfs device'
insmod "$MODULE"
grep -q '^lab2_driver ' /proc/modules
[ -r /proc/lab2_info ]
[ -r /sys/class/lab2_class/lab2/buffer_len ]
echo '[2] insmod recreated the driver interfaces'
echo '=== Lifecycle Test PASSED ==='
