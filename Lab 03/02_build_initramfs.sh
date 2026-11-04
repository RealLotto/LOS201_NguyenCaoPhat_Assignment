#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
LAB1="${LAB1:-$HOME/embedded_lab1}"
LAB2="${LAB2:-$HOME/embedded_lab2}"
LAB3="${LAB3:-$SCRIPT_DIR}"
ROOTFS="$LAB3/rootfs/initramfs"
ARCHIVE="$LAB3/output/initramfs_lab3.cpio.gz"
BUNDLED_GDBSERVER="$LAB3/gdbserver/armhf-rootfs"

"$SCRIPT_DIR/00_check_prerequisites.sh"
for binary in buggy_app leaky_app_arm perf_target_arm; do
  if [[ ! -s "$LAB3/apps/$binary" ]]; then
    echo "Build applications first; missing $LAB3/apps/$binary" >&2
    exit 1
  fi
done

rm -rf -- "$ROOTFS"
mkdir -p "$ROOTFS" "$LAB3/output"
(
  cd "$ROOTFS"
  gzip -dc "$LAB2/output/initramfs_lab2.cpio.gz" |
    fakeroot cpio -id --quiet
)

install -m 0755 "$LAB3/apps/buggy_app" "$ROOTFS/usr/bin/buggy_app"
install -m 0755 "$LAB3/apps/leaky_app_arm" "$ROOTFS/usr/bin/leaky_app_arm"
install -m 0755 "$LAB3/apps/perf_target_arm" "$ROOTFS/usr/bin/perf_target_arm"

if [[ -n "${GDBSERVER_ARM:-}" ]]; then
  if [[ ! -s "$GDBSERVER_ARM" ]] || [[ "$(file -b "$GDBSERVER_ARM")" != *"ARM"* ]]; then
    echo "GDBSERVER_ARM must name an existing ARM executable: $GDBSERVER_ARM" >&2
    exit 1
  fi
  install -m 0755 "$GDBSERVER_ARM" "$ROOTFS/usr/bin/gdbserver"
  printf 'ARM gdbserver installed from %s\n' "$GDBSERVER_ARM" \
    > "$LAB3/logs/gdbserver_target_status.txt"
elif [[ -s "$BUNDLED_GDBSERVER/usr/bin/gdbserver" ]]; then
  cp -a "$BUNDLED_GDBSERVER/lib/." "$ROOTFS/lib/"
  cp -a "$BUNDLED_GDBSERVER/usr/lib/." "$ROOTFS/usr/lib/"
  install -m 0755 "$BUNDLED_GDBSERVER/usr/bin/gdbserver" \
    "$ROOTFS/usr/bin/gdbserver"
  printf '%s\n' \
    'ARM gdbserver 12.1 and Ubuntu 22.04 armhf runtime libraries included.' \
    > "$LAB3/logs/gdbserver_target_status.txt"
else
  printf '%s\n' \
    "ARM gdbserver package missing; provide one with GDBSERVER_ARM=/path/to/gdbserver and rebuild." \
    > "$LAB3/logs/gdbserver_target_status.txt"
fi

for path in \
  "$ROOTFS/lib/modules/5.15.0/lab2_driver.ko" \
  "$ROOTFS/etc/init.d/rcS" \
  "$ROOTFS/usr/bin/gdbserver" \
  "$ROOTFS/usr/bin/test_driver.sh" \
  "$ROOTFS/usr/bin/test_procfs.sh"; do
  if [[ ! -s "$path" ]]; then
    echo "LAB-02 rootfs content was lost while layering LAB-03: $path" >&2
    exit 1
  fi
done

fakeroot bash -c '
  set -euo pipefail
  rootfs=$1
  archive=$2
  cd "$rootfs"
  rm -f dev/console dev/null dev/zero dev/random
  mknod -m 600 dev/console c 5 1
  mknod -m 666 dev/null c 1 3
  mknod -m 666 dev/zero c 1 5
  mknod -m 444 dev/random c 1 8
  find . -print0 | cpio --null -o -H newc -R 0:0 --quiet |
    gzip -n -9 > "$archive"
' _ "$ROOTFS" "$ARCHIVE"
gzip -t "$ARCHIVE"
gzip -dc "$ARCHIVE" | cpio -it --quiet 2>/dev/null |
  grep -E '(^|/)(buggy_app|gdbserver|leaky_app_arm|perf_target_arm)$|lab2_driver\.ko|test_(driver|procfs)\.sh' \
  > "$LAB3/logs/initramfs_inventory.txt"
cat "$LAB3/logs/initramfs_inventory.txt"
ls -lh "$ARCHIVE"
echo "LAB-03 initramfs built on the unmodified LAB-02 rootfs."
