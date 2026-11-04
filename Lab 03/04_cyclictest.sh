#!/usr/bin/env bash
set -uo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
LOG_DIR="$SCRIPT_DIR/logs"
mkdir -p "$LOG_DIR"

if ! command -v cyclictest >/dev/null 2>&1; then
  printf 'NOT RUN: cyclictest is not installed. Install rt-tests, then rerun this script.\n' |
    tee "$LOG_DIR/cyclictest_status.txt"
  exit 0
fi

if cyclictest --mlockall --threads=4 --priority=80 --interval=1000 \
  --distance=500 --loops=10000 --histogram=200 \
  --histfile="$LOG_DIR/hist_standard.txt" \
  > "$LOG_DIR/cyclictest_standard.txt" 2>&1; then
  echo "cyclictest completed with SCHED_FIFO priority 80."
elif command -v sudo >/dev/null 2>&1 && sudo -n true 2>/dev/null; then
  if sudo -n cyclictest --mlockall --threads=4 --priority=80 --interval=1000 \
    --distance=500 --loops=10000 --histogram=200 \
    --histfile="$LOG_DIR/hist_standard.txt" \
    > "$LOG_DIR/cyclictest_standard.txt" 2>&1; then
    echo "cyclictest completed with SCHED_FIFO priority 80 via sudo."
  else
    printf 'cyclictest failed; see cyclictest_standard.txt.\n' |
      tee "$LOG_DIR/cyclictest_status.txt"
    exit 1
  fi
else
  printf 'Could not obtain real-time scheduling permission. Re-run with sudo; no RT test was reported.\n' |
    tee "$LOG_DIR/cyclictest_status.txt"
  exit 0
fi

if command -v gnuplot >/dev/null 2>&1; then
  gnuplot "$SCRIPT_DIR/plot_latency.gnuplot"
else
  printf 'Histogram data collected; gnuplot is not installed, so no PNG was generated.\n' |
    tee "$LOG_DIR/gnuplot_status.txt"
fi
