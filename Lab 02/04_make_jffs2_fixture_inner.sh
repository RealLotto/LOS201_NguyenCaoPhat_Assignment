#!/usr/bin/env bash
set -euo pipefail

LAB2="$HOME/embedded_lab2"
DATA="$LAB2/mtd/jffs2-data"
IMAGE="$LAB2/mtd/rootfs.jffs2"
LOG="$LAB2/logs/jffs2_image_build.txt"

mkdir -p "$DATA/logs" "$LAB2/logs"
printf 'Hello from JFFS2!\n' > "$DATA/hello.txt"
printf 'Embedded Linux Lab2\n' > "$DATA/info.txt"
date -Is > "$DATA/logs/boot.log"

{
  echo "Building an offline JFFS2 image fixture (no MTD device is accessed)."
  mkfs.jffs2 --root="$DATA" --eraseblock=0x4000 \
    --output="$IMAGE" --pad --no-cleanmarkers
  ls -lh "$IMAGE"
  magic="$(od -An -tx1 -N2 "$IMAGE" | xargs)"
  echo "Image prefix: $magic"
  if [[ "$magic" != "85 19" ]]; then
    echo "ERROR: expected JFFS2 magic 85 19." >&2
    exit 1
  fi
  echo "Offline JFFS2 image checks passed."
} 2>&1 | tee "$LOG"
