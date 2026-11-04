LAB-03 - Kernel debugging and embedded application performance
Student: Nguyen Cao Phat (SE201044)

Compatibility
-------------
LAB-03 reuses, without rebuilding or modifying:
  ~/embedded_lab1/output/{zImage,vexpress-v2p-ca9.dtb}
  ~/embedded_lab1/kernel/linux-5.15
  ~/embedded_lab2/output/initramfs_lab2.cpio.gz

The prerequisite check confirms the kernel is 5.15.0 with CONFIG_MODULES=y,
that LAB-01 and LAB-02 use identical zImage/DTB files, and that the LAB-02
archive contains lab2_driver.ko built for the 5.15.0 kernel. LAB-03 extracts
that archive to its own rootfs before adding applications; it never edits
LAB-02 files or its boot archive.

Build and run
-------------
From this directory:
  chmod +x ./*.sh
  ./00_check_prerequisites.sh
  ./01_build.sh
  ./02_build_initramfs.sh
  ./run_qemu_tests.py

The QEMU test boots the LAB-01 kernel and DTB with the LAB-03 initramfs,
reruns the LAB-02 character-device and procfs/sysfs tests, and runs the
fixed ARM memory app and ARM profiling workload. Logs are saved in logs/.

Host-side analysis:
  ./03_host_analysis.sh

This runs the intentional-leak and fixed variants, Valgrind when installed,
strace, and perf. perf may require host permissions or kernel support; an
unavailable measurement is recorded rather than presented as a result.

Real-time measurement (on the host, never in QEMU):
  sudo ./04_cyclictest.sh

Install rt-tests and gnuplot if needed. The script records a cyclictest
histogram only when the measurement succeeds with real-time scheduling
permissions. A PREEMPT_RT/low-latency comparison requires booting a matching
RT kernel and rerunning cyclictest; expected ranges in the handout are not
substituted for measured results.

After saving both `logs/hist_standard.txt` and `logs/hist_realtime.txt`, run:
  gnuplot ./plot_latency.gnuplot
This generates `logs/latency_histogram.png`, with normalized standard vs
PREEMPT_RT distributions, a zoomed common-latency view, and a separate tail
view. The plotted means and maxima are calculated from the histogram inputs.

Remote GDB
----------
apps/buggy_app.c provides isolated, repeatable cases:
  buggy_app --case null
  buggy_app --case oob
  buggy_app --case uaf
  buggy_app --case overflow

The LAB-03 initramfs bundles ARM gdbserver. Rebuild it after changing the
image, then fully stop and restart QEMU with port forwarding enabled; network
forwarding cannot be added to an already-running QEMU process. From this
directory, start QEMU on the host:
  qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 -nographic \
    -kernel ~/embedded_lab1/output/zImage \
    -dtb ~/embedded_lab1/output/vexpress-v2p-ca9.dtb \
    -initrd output/initramfs_lab3.cpio.gz \
    -append 'console=ttyAMA0,115200 rdinit=/sbin/init mem=512M' \
    -net nic,model=lan9118 \
    -net user,hostfwd=tcp::1235-:1234

Here host TCP port 1235 forwards to guest TCP port 1234. If host port 1235
is already in use, choose a free host port (for example 1236) in the QEMU
hostfwd option and use that same port in GDB below. Do not start a second
QEMU instance on a host port already used by another process.

In the QEMU guest's BusyBox shell, bring up the guest NIC and start gdbserver
in the foreground. Leave it running while using GDB:
  ifconfig eth0 10.0.2.15 netmask 255.255.255.0 up
  gdbserver 0.0.0.0:1234 /usr/bin/buggy_app --case oob

Wait for "Listening on port 1234". If the command reports that gdbserver is
missing, rebuild the LAB-03 image with ./02_build_initramfs.sh and reboot
QEMU using output/initramfs_lab3.cpio.gz. The older image or a QEMU process
that was not restarted will not contain the bundled server or port forward.

In a second host terminal, from this project directory, connect to the
forwarded host port (not the guest port):
  gdb-multiarch apps/buggy_app
  (gdb) set architecture arm
  (gdb) target remote localhost:1235
  (gdb) break apps/buggy_app.c:10 if i == len
  (gdb) continue
  (gdb) print i
  (gdb) print data[5]
  (gdb) backtrace
  (gdb) info registers

Use the port selected in QEMU's hostfwd option instead of 1235 if you changed
it. Do not run "continue" until GDB reports "Remote debugging using
localhost:PORT" and the breakpoint has been set. The conditional breakpoint
is on the loop body: a breakpoint on the entry to process_data occurs before
the loop variable i is in scope. At the conditional breakpoint, i == len == 5,
which is the first out-of-bounds iteration in this test case.

Troubleshooting:
  - "Connection timed out": check that QEMU was restarted with the hostfwd
    option, the guest gdbserver is still running and listening on 1234, and
    GDB uses the forwarded host port.
  - Unexpected GDB packet/protocol replies: verify the host port is the QEMU
    forward to guest port 1234, rather than another service.
  - "The program is not being run": the remote target connection did not
    succeed; reconnect successfully before setting a breakpoint and continuing.

If replacing the bundled target server, it must be an ARMv7 binary. Rebuild
the initramfs with:
  GDBSERVER_ARM=/path/to/armv7/gdbserver ./02_build_initramfs.sh
The script rejects a non-ARM gdbserver. The host needs gdb-multiarch.
When an ARM gdbserver is unavailable, the handout permits QEMU user-mode as
a fallback:
  ./06_qemu_user_gdb.sh
  ./06_qemu_user_gdb.sh null
  ./06_qemu_user_gdb.sh uaf
  ./06_qemu_user_gdb.sh overflow
This requires host qemu-user and gdb-multiarch and saves its session under
logs/gdb_<case>_session.txt (the default out-of-bounds case also writes
logs/gdb_session.txt). It debugs the same ARM ELF outside the system-mode
guest; the LAB-03/LAB-02 system-mode integration test remains separate.

Applications
------------
  apps/buggy_app.c     ARM GDB target with four isolated defects
  apps/leaky_app.c     Host Valgrind target; --leak and --fixed modes
  apps/perf_target.c   Host/ARM CPU, linear-search, and file-I/O workload
  Makefile             Builds debug host and ARM binaries

Build outputs are kept under this project:
  output/initramfs_lab3.cpio.gz
  rootfs/initramfs/
  logs/

Submission:
  ./05_package.sh
creates SE201044_NguyenCaoPhat_Lab03.tar.gz with the required top-level
directory, applications, initramfs, logs, report, configs, and scripts.

Report
------
The editable report is bao_cao/SE201044_Lab03_BaoCao.html. Regenerate its PDF:
  libreoffice --headless --convert-to pdf --outdir bao_cao \
    bao_cao/SE201044_Lab03_BaoCao.html

Don't claim measurements that were not captured. In particular, cyclictest,
perf, and remote GDB can depend on host permissions and optional tools.
