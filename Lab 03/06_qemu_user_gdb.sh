#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
LOG_DIR="$SCRIPT_DIR/logs"
TARGET="$SCRIPT_DIR/apps/buggy_app"
PORT="${GDB_PORT:-1234}"
CASE_NAME="${1:-oob}"
QEMU_PID=""
mkdir -p "$LOG_DIR"

case "$CASE_NAME" in
  oob)
    GDB_COMMANDS=(
      -ex 'break apps/buggy_app.c:10 if i == len'
      -ex continue
      -ex 'print i'
      -ex 'print len'
      -ex 'print data[i]'
      -ex backtrace
      -ex 'info registers pc sp lr'
    )
    EXPECTED='Breakpoint .*apps/buggy_app.c:10'
    ;;
  null)
    GDB_COMMANDS=(
      -ex 'catch signal SIGSEGV'
      -ex continue
      -ex 'print value'
      -ex backtrace
      -ex 'info registers pc sp lr'
    )
    EXPECTED='SIGSEGV'
    ;;
  uaf)
    GDB_COMMANDS=(
      -ex 'break apps/buggy_app.c:63'
      -ex continue
      -ex 'print message'
      -ex 'x/s message'
      -ex backtrace
      -ex 'info registers pc sp lr'
    )
    EXPECTED='message'
    ;;
  overflow)
    GDB_COMMANDS=(
      -ex 'break apps/buggy_app.c:36'
      -ex continue
      -ex 'print a'
      -ex 'print b'
      -ex next
      -ex 'print result'
      -ex backtrace
      -ex 'info registers pc sp lr'
    )
    EXPECTED='100000'
    ;;
  *)
    echo "Usage: $0 [null|oob|uaf|overflow]" >&2
    exit 2
    ;;
esac

ulimit -c 0

for command_name in qemu-arm gdb-multiarch; do
  if ! command -v "$command_name" >/dev/null 2>&1; then
    printf 'NOT RUN: %s is required for QEMU user-mode remote debugging.\n' \
      "$command_name" | tee "$LOG_DIR/gdb_status.txt"
    exit 0
  fi
done
if [[ ! -s "$TARGET" ]]; then
  echo "Build the ARM applications first: $TARGET" >&2
  exit 1
fi

cleanup()
{
  if [[ -n "$QEMU_PID" ]] && kill -0 "$QEMU_PID" 2>/dev/null; then
    kill "$QEMU_PID"
    wait "$QEMU_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT

GDB_LOG="$LOG_DIR/gdb_${CASE_NAME}_session.txt"
qemu-arm -g "$PORT" "$TARGET" --case "$CASE_NAME" \
  > "$LOG_DIR/qemu_user_gdb_${CASE_NAME}_target.txt" 2>&1 &
QEMU_PID=$!
sleep 1
gdb-multiarch -q "$TARGET" -batch \
  -ex "set pagination off" \
  -ex "set architecture arm" \
  -ex "target remote localhost:$PORT" \
  "${GDB_COMMANDS[@]}" \
  -ex detach \
  > "$GDB_LOG" 2>&1

if [[ "$CASE_NAME" == oob ]]; then
  cp "$GDB_LOG" "$LOG_DIR/gdb_session.txt"
fi

if ! grep -qE "$EXPECTED" "$GDB_LOG"; then
  echo "GDB did not observe the $CASE_NAME case; see $GDB_LOG." >&2
  exit 1
fi
wait "$QEMU_PID" || true
QEMU_PID=""
echo "QEMU user-mode ARM remote GDB session passed for $CASE_NAME."
