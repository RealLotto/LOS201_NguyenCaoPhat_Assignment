#!/bin/bash
# Part C.2-C.4: build rootfs tree and pack initramfs.cpio.gz (root or fakeroot)
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [[ $EUID -ne 0 && -z ${FAKEROOTKEY:-} ]]; then
  if command -v fakeroot >/dev/null 2>&1; then
    exec fakeroot bash "$0" "$@"
  elif command -v podman >/dev/null 2>&1 && podman image exists lab1-build; then
    exec "$SCRIPT_DIR/container_run.sh" fakeroot bash /scripts/05_make_rootfs.sh "$@"
  else
    echo "Device nodes require root or fakeroot. Run ./00_setup_ubuntu.sh to prepare the build container." >&2
    exit 1
  fi
fi

L=$HOME/embedded_lab1
BB=$L/rootfs/busybox-1.35.0/_install
R=$L/rootfs/initramfs
mkdir -p $R && cd $R
mkdir -p bin sbin usr/bin usr/sbin dev proc sys tmp etc/init.d root lib/arm-linux-gnueabihf
cp -a $BB/bin/* bin/; cp -a $BB/sbin/* sbin/; cp -a $BB/usr/* usr/

cat > init << 'X'
#!/bin/sh
mount -t proc none /proc
mount -t sysfs none /sys
exec /sbin/init
X
chmod +x init

cat > etc/inittab << 'X'
::sysinit:/etc/init.d/rcS
::askfirst:-/bin/sh
::ctrlaltdel:/sbin/reboot
::shutdown:/sbin/swapoff -a
::shutdown:/bin/umount -a -r
::restart:/sbin/init
X

cat > etc/init.d/rcS << 'X'
#!/bin/sh
mount -t proc none /proc
mount -t sysfs none /sys
mount -t tmpfs none /tmp
echo 'Boot completed! Welcome to Embedded Linux Lab 1'
hostname embedded-lab
X
chmod +x etc/init.d/rcS

echo 'root:x:0:0:root:/root:/bin/sh' > etc/passwd
echo 'embedded-lab' > etc/hostname

rm -f dev/null dev/console dev/zero dev/random
mknod -m 666 dev/null c 1 3
mknod -m 600 dev/console c 5 1
mknod -m 666 dev/zero c 1 5
mknod -m 444 dev/random c 1 8

file bin/busybox | tee $L/logs/C_rootfs_check.log
ls -la . | tee -a $L/logs/C_rootfs_check.log
find . | cpio -H newc -o -R 0:0 2>/dev/null | gzip > $L/output/initramfs.cpio.gz
ls -lh $L/output/initramfs.cpio.gz
echo "initramfs done."
