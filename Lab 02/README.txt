
LAB-02 — ARMv7 character driver, procfs/sysfs, JFFS2, BusyBox integration
Student: NguyenCaoPhat (SE201044)

Prerequisite
------------
Complete LAB-01 first. This project checks and reuses:
  ~/embedded_lab1/kernel/linux-5.15
  ~/embedded_lab1/output/{zImage,vexpress-v2p-ca9.dtb,initramfs.cpio.gz}
  ~/embedded_lab1/configs/{kernel.config,busybox.config}

The LAB-01 kernel must be version 5.15.0 with CONFIG_MODULES=y. LAB-02 builds
the module against that exact kernel build and verifies its vermagic before
booting. It uses the existing lab1-build Podman image to cross-compile; a
small derived image installs mtd-utils for offline JFFS2-image generation.

Build and test
--------------
  cd ~/Documents/Lab/lab2_files
  ./00_setup.sh
  ./01_build_driver.sh
  ./02_build_initramfs.sh
  ./03_test_qemu.sh

03_test_qemu.sh runs the LAB-01 zImage and DTB with the LAB-02 initramfs. It
captures a boot log, auto-load check, character-device read/write and seek,
reverse-read mode, overwrite counter, procfs and sysfs values, dmesg,
optional/required module-failure paths, and module unload/reload in
~/embedded_lab2/logs/.
For a manual shell instead, run ./06_boot_lab2.sh [1-4].

The source and editable BusyBox overlay are in driver/ and rootfs/overlay/.
The generated submission rootfs is ~/embedded_lab2/rootfs/initramfs. The
final bootable archive is ~/embedded_lab2/output/initramfs_lab2.cpio.gz.

JFFS2/MTD
---------
  ./04_make_jffs2_fixture.sh
creates and checks a JFFS2 image entirely offline; it never touches an MTD
device. To run the destructive MTD test on an authorized Ubuntu host:
  sudo ./run_mtd_jffs2.sh

The host script refuses to run as an unprivileged user, refuses to reuse an
already-loaded nandsim, and only erases the NAND-simulator partition it loaded
itself. It does not choose /dev/mtd0 by number. Do not remove these safeguards:
real MTD devices may contain firmware or other users' data.

Submission
----------
The report PDF is bao_cao/SE201044_Lab02_BaoCao.pdf, with editable HTML source
beside it. Regenerate the PDF with:
  libreoffice --headless --convert-to pdf --outdir bao_cao \
    bao_cao/SE201044_Lab02_BaoCao.html

After building and testing:
  ./07_pack.sh SE201044_NguyenCaoPhat_Lab02

This creates SE201044_NguyenCaoPhat_Lab02.tar.gz in this project folder.
The package contains the module source and ARM binary, unpacked initramfs
(including proper /dev character nodes), bootable cpio.gz, report, configs,
and test logs.
