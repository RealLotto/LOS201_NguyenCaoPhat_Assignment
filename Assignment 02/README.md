# Assignment 02 — Sensor Monitoring System

## Contents

- `driver/`: misc sensor module, shared ioctl definition, and kernel Makefile.
- `app/`: C11/POSIX-thread application, queue-based alert/logger paths, bounded
  circular buffer, and host/ARM build targets.
- `test_sms.sh`: QEMU guest smoke/evidence script.
- `bao_cao/Assignment_2_Report_SE201044.docx`: implementation guide, measured
  QEMU results, and remaining limitations.

## Build

Run the following from the repository root.

Build the userspace host binary for Valgrind/strace/perf:

```sh
cd "Assignment 02"
make -C app host
make -C app arm ARM_CC=arm-linux-gnueabihf-gcc
make -C driver KDIR=/path/to/kernel/build ARCH=arm \
  CROSS_COMPILE=arm-linux-gnueabihf-
```

Run `sh test_sms.sh` inside the root QEMU guest after both ARM artifacts are
available. Use `-s other` for unprivileged host profiling; use `-s fifo` with
the required privileges for the real-time comparison.

The ARM driver and application were built for Buildroot Linux 6.18.7, and the
SMS QEMU test passed with exit status 0. Driver statistics, memory snapshots,
maps, dmesg, and one 119-sample SCHED_FIFO jitter run are in `logs/`. The
SCHED_OTHER comparison, 1,000-sample jitter target, Valgrind/GDB/strace/perf
results, and demo video remain outstanding. This generic guest is not a
course-provided image or physical target. The reviewed runtime logs are
included; generated binaries are excluded from Git.
