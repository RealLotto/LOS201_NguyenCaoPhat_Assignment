#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PACKAGE_NAME="${1:-SE201044_NguyenCaoPhat_Lab03}"
PACKAGE="$SCRIPT_DIR/${PACKAGE_NAME}.tar.gz"
STAGING_DIR="$(mktemp -d)"
trap 'rm -rf -- "$STAGING_DIR"' EXIT

for path in \
  "$SCRIPT_DIR/apps/buggy_app.c" \
  "$SCRIPT_DIR/apps/leaky_app.c" \
  "$SCRIPT_DIR/apps/perf_target.c" \
  "$SCRIPT_DIR/apps/buggy_app" \
  "$SCRIPT_DIR/output/initramfs_lab3.cpio.gz" \
  "$SCRIPT_DIR/bao_cao/${PACKAGE_NAME%_NguyenCaoPhat_Lab03}_Lab03_BaoCao.pdf"; do
  if [[ ! -s "$path" ]]; then
    echo "Required submission item is missing or empty: $path" >&2
    exit 1
  fi
done

STAGED="$STAGING_DIR/$PACKAGE_NAME"
mkdir -p "$STAGED"/{apps,bao_cao,configs,logs,output,project-scripts}
cp "$SCRIPT_DIR/apps/"*.c "$SCRIPT_DIR/apps/buggy_app" \
  "$SCRIPT_DIR/apps/"*arm \
  "$SCRIPT_DIR/apps/leaky_app" "$SCRIPT_DIR/apps/leaky_app_fixed" \
  "$SCRIPT_DIR/apps/perf_target" "$STAGED/apps/"
cp "$SCRIPT_DIR/output/initramfs_lab3.cpio.gz" "$STAGED/output/"
cp "$SCRIPT_DIR/configs/"* "$STAGED/configs/"
cp -a "$SCRIPT_DIR/gdbserver" "$STAGED/"
cp "$SCRIPT_DIR/bao_cao/"*.html "$SCRIPT_DIR/bao_cao/"*.pdf "$STAGED/bao_cao/"
cp "$SCRIPT_DIR/README.txt" "$STAGED/"
cp -a "$SCRIPT_DIR/logs/." "$STAGED/logs/"
cp "$SCRIPT_DIR/"*.sh "$SCRIPT_DIR/"*.py "$SCRIPT_DIR/Makefile" \
  "$SCRIPT_DIR/plot_latency.gnuplot" "$STAGED/project-scripts/"

rm -f "$PACKAGE"
tar -czf "$PACKAGE" -C "$STAGING_DIR" "$PACKAGE_NAME"
tar -tzf "$PACKAGE" | sed -n '1,80p'
echo "Created $PACKAGE"
