# Assignment 01 — Device Drivers

## Contents

- `task1_ioctl/`: fixed-major character device `asgn1_driver`, shared ioctl ABI,
  userspace helper, Makefile, and QEMU test script.
- `task2_misc/`: `counter_driver` misc device, procfs statistics, Makefile, and test.
- `bao_cao/Assignment_1_Report_SE201044.docx`: design analysis, algorithm notes,
  theory answers, and QEMU test evidence.

## Build and run

Run on the QEMU ARMv7 guest with matching kernel headers/configuration. For
example, from the relevant source folder:

```sh
make KDIR=/path/to/kernel/build ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
```

For Task 1, build the ioctl userspace helper with `make helper`; then run
`sh test_asgn1.sh` as root. For Task 2, run `sh test_counter.sh` as root.
The proc interfaces select `struct proc_ops` on Linux 5.6+ and
`struct file_operations` on older kernels.

Both ARM modules and the ioctl helper were built for Buildroot Linux 6.18.7.
The ioctl and counter QEMU tests passed with exit status 0; their complete
transcripts are in `logs/test_asgn1_output.txt` and
`logs/test_counter_output.txt`. The guest used major 242 because its kernel
already registered major 241. This is a generic Buildroot guest, not a
course-provided image or physical target.
