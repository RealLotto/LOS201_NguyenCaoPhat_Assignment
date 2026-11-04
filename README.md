# LOS201 Embedded Linux

This repository contains two course assignments and three hands-on labs for
Embedded Linux. Each folder is a separate workspace with its own source,
build instructions, reports, and available test evidence.

## Assignments

- [Assignment 01](./Assignment%2001/) — ioctl character device and misc counter.
- [Assignment 02](./Assignment%2002/) — sensor driver and multithreaded SMS application.

## Labs

- [Lab 01](./Lab%2001/) — build an ARMv7 Linux kernel, U-Boot, BusyBox root
  filesystem, and initramfs; boot and inspect the system with QEMU.
- [Lab 02](./Lab%2002/) — implement and test an ARM character driver with
  procfs/sysfs interfaces; build a JFFS2 image and integrate the driver into
  the LAB-01 system.
- [Lab 03](./Lab%2003/) — debug ARM applications with GDB, inspect memory
  behavior with Valgrind, trace system calls, and compare cyclictest latency
  on standard and PREEMPT_RT host kernels.

The lab directories are siblings of the assignment directories. LAB-02 builds
on the kernel and QEMU setup from LAB-01, and LAB-03 reuses and checks the
LAB-01/LAB-02 artifacts. Follow the README in each folder for its prerequisites,
commands, and test limitations.

## Repository notes

The original lab handouts and some course materials are kept outside this
repository. Supporting files are included where they form part of a lab's
documented evidence or deliverables; consult each lab's report and README for
details. Test results and any unavailable or unmeasured items are described in
the corresponding workspace rather than assumed to have passed.
