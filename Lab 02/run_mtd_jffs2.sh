#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
HOME_DIR="${HOME:?HOME is not set}"
LAB2="$HOME_DIR/embedded_lab2"
LOG="$LAB2/logs/mtd_jffs2.txt"
TEMP_DIR=""
MTD_NAME=""
MTD_DEVICE=""
MTD_BLOCK=""
MOUNT_DIR=""
MOUNTED=0
OWN_NANDSIM=0

mkdir -p "$LAB2/logs"
exec > >(tee "$LOG") 2>&1
echo "=== LAB-02 MTD/JFFS2 host test ==="
echo "Host kernel: $(uname -r)"
echo "--- Existing MTD inventory (read-only) ---"
cat /proc/mtd 2>/dev/null || echo "No /proc/mtd"

if [[ $EUID -ne 0 ]]; then
  echo "ERROR: this test must run as root (for example: sudo $SCRIPT_DIR/run_mtd_jffs2.sh)."
  echo "No MTD device was modified."
  exit 1
fi

for command_name in modprobe rmmod mtdinfo flash_erase mkfs.jffs2 nandwrite \
  nanddump mount umount hexdump; do
  if ! command -v "$command_name" >/dev/null 2>&1; then
    echo "ERROR: required host command '$command_name' is missing; install mtd-utils and host tools."
    exit 1
  fi
done

if [[ -d /sys/module/nandsim ]]; then
  echo "ERROR: nandsim is already loaded by another session."
  echo "Refusing to erase or unload an MTD device that this script did not create."
  exit 1
fi

cleanup()
{
  status=$?
  if [[ $MOUNTED -eq 1 ]]; then
    umount "$MOUNT_DIR" || echo "WARNING: could not unmount $MOUNT_DIR during cleanup"
  fi
  if [[ $OWN_NANDSIM -eq 1 ]] && [[ -d /sys/module/nandsim ]]; then
    rmmod nandsim || echo "WARNING: could not unload the nandsim instance created by this script"
  fi
  if [[ -n $TEMP_DIR && -d $TEMP_DIR ]]; then
    rm -rf "$TEMP_DIR"
  fi
  if [[ $OWN_NANDSIM -eq 1 ]]; then
    echo "Cleaned up the nandsim instance created by this run (status=$status)."
  fi
  return "$status"
}
trap cleanup EXIT

modprobe nandsim first_id_byte=0x20 second_id_byte=0x35
OWN_NANDSIM=1
modprobe mtdblock
modprobe jffs2

while IFS= read -r entry; do
  [[ $entry == *'"NAND simulator partition 0"'* ]] || continue
  MTD_NAME="${entry%%:*}"
  break
done < /proc/mtd

if [[ ! $MTD_NAME =~ ^mtd[0-9]+$ ]]; then
  echo "ERROR: could not identify the NAND simulator partition created by this run."
  exit 1
fi

MTD_DEVICE="/dev/$MTD_NAME"
MTD_BLOCK="/dev/mtdblock${MTD_NAME#mtd}"
if [[ ! -c $MTD_DEVICE || ! -b $MTD_BLOCK ]]; then
  echo "ERROR: simulator device nodes are unavailable: $MTD_DEVICE, $MTD_BLOCK"
  exit 1
fi

ERASE_SIZE="$(<"/sys/class/mtd/$MTD_NAME/erasesize")"
echo "--- Simulator details ---"
echo "MTD device: $MTD_DEVICE"
echo "MTD block: $MTD_BLOCK"
echo "Erase size: $ERASE_SIZE bytes"
mtdinfo "$MTD_DEVICE"
cat /proc/mtd

TEMP_DIR="$(mktemp -d /tmp/lab2-jffs2.XXXXXX)"
MOUNT_DIR="$TEMP_DIR/mnt"
DATA_DIR="$TEMP_DIR/data"
IMAGE="$TEMP_DIR/rootfs.jffs2"
DUMP="$TEMP_DIR/dump.bin"
mkdir -p "$MOUNT_DIR" "$DATA_DIR/logs"
printf 'Hello from JFFS2!\n' > "$DATA_DIR/hello.txt"
printf 'Embedded Linux Lab2\n' > "$DATA_DIR/info.txt"
date -Is > "$DATA_DIR/logs/boot.log"

echo "--- Erase simulator flash ---"
flash_erase "$MTD_DEVICE" 0 0
echo "--- Build JFFS2 image (eraseblock=$ERASE_SIZE) ---"
mkfs.jffs2 --root="$DATA_DIR" --eraseblock="$ERASE_SIZE" \
  --output="$IMAGE" --pad --no-cleanmarkers
ls -lh "$IMAGE"

echo "--- Write image to the simulator ---"
nandwrite -p "$MTD_DEVICE" "$IMAGE"
echo "--- Mount JFFS2 ---"
mount -t jffs2 "$MTD_BLOCK" "$MOUNT_DIR"
MOUNTED=1
cat "$MOUNT_DIR/hello.txt"
cat "$MOUNT_DIR/info.txt"
cat "$MOUNT_DIR/logs/boot.log"

printf 'Written at %s\n' "$(date -Is)" >> "$MOUNT_DIR/logs/boot.log"
printf 'New runtime file\n' > "$MOUNT_DIR/runtime.txt"
echo "--- Remount and verify persistent data ---"
umount "$MOUNT_DIR"
MOUNTED=0
mount -t jffs2 "$MTD_BLOCK" "$MOUNT_DIR"
MOUNTED=1
grep -F 'Written at ' "$MOUNT_DIR/logs/boot.log"
grep -Fx 'New runtime file' "$MOUNT_DIR/runtime.txt"

echo "--- Dump raw NAND data ---"
nanddump -o -l 512 -f "$DUMP" "$MTD_DEVICE"
hexdump -C "$DUMP" | head -20
MAGIC="$(od -An -tx1 -N2 "$DUMP" | tr -s ' ' | sed 's/^ //;s/ $//')"
if [[ $MAGIC != "85 19" ]]; then
  echo "ERROR: expected JFFS2 magic 85 19; found '$MAGIC'."
  exit 1
fi
echo "JFFS2 magic verified: $MAGIC"
umount "$MOUNT_DIR"
MOUNTED=0
echo "=== MTD/JFFS2 persistence test PASSED ==="
