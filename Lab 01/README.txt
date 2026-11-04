LAB-01 scripts for an Ubuntu host (Ubuntu 22.04 or newer).

First, make the scripts executable and install host tools/build the Ubuntu
cross-compilation container:
  cd ~/Documents/Lab/lab1_files
  chmod +x ./*.sh
  ./00_setup_ubuntu.sh

Then run the steps in order:
  ./01_download.sh                     # sources -> ~/embedded_lab1
  ./build.sh 02_build_kernel.sh        # Part A (10-30 min)
  ./build.sh 03_build_uboot.sh         # Part B.1
  ./build.sh 04_build_busybox.sh       # Part C.1
  ./05_make_rootfs.sh                  # Part C.2-C.4 (fakeroot for device nodes)
  ./06_boot_uboot.sh                   # Part B.2/B.3 (see uboot_commands.txt)
  ./07_boot_no_rootfs.sh               # Part B.4 (kernel panic is expected)
  ./08_boot_full.sh [cpus]             # Part C.5 (see verify_commands.txt)
  ./09_package.sh SE201044_NguyenCaoPhat

The build scripts use a local ARM cross-toolchain when available; otherwise
they build in the podman container created by 00_setup_ubuntu.sh. QEMU runs
on the Ubuntu host.

Create the report PDF from the included HTML source:
  libreoffice --headless --convert-to pdf --outdir bao_cao \
    bao_cao/SE201044_Lab01_BaoCao.html

The package script requires a report PDF in this project's bao_cao/ folder
and creates
SE201044_NguyenCaoPhat_Lab01.tar.gz in this project folder with the required
top-level SE201044_NguyenCaoPhat_Lab01/ directory and output/, configs/,
rootfs/initramfs/, and bao_cao/ beneath it.

LAB-01 coverage:
  Part A: Linux 5.15 vexpress_defconfig, ARM zImage + DTB, kernel config and logs.
  Part B: U-Boot 2022.04 build and interactive shell commands; separate
          no-rootfs boot records the expected kernel panic.
  Part C: static ARM BusyBox, initramfs, QEMU boot, and shell verification.

On QEMU, `saveenv` may report a flash sector-boundary error because this
machine has no persistent environment storage; the in-session variables
remain available until QEMU exits.

The kernel build does not pass the handout's hard-float KCFLAGS to kernel C
sources: ARM Linux kernel code uses the kernel's soft-float calling convention.
It sets KCFLAGS=-march=armv7-a to work around the Linux 5.15/toolchain
combination selecting an ARMv5 assembler target for ARMv7 instructions. This
does not enable hard-float. The specified ARMv7 hard-float flags are applied
to BusyBox.

Save screenshots or text evidence for the report while running the lab.
Required report evidence includes the kernel config/build, U-Boot commands,
the expected panic, successful QEMU boot, and uname/mount/ps output. The
included scripts save build and boot logs under ~/embedded_lab1/logs.
LAB-02 can reuse ~/embedded_lab1/{output,rootfs/initramfs,kernel/linux-5.15}.
