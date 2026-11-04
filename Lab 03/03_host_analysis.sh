#!/usr/bin/env bash
set -uo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
LOG_DIR="$SCRIPT_DIR/logs"
mkdir -p "$LOG_DIR"

make -C "$SCRIPT_DIR" host || exit $?

"$SCRIPT_DIR/apps/leaky_app" --leak \
  > "$LOG_DIR/leaky_app_output.txt" 2>&1
"$SCRIPT_DIR/apps/leaky_app_fixed" \
  > "$LOG_DIR/leaky_app_fixed_output.txt" 2>&1

if command -v valgrind >/dev/null 2>&1; then
  valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all \
    --track-origins=yes --log-file="$LOG_DIR/valgrind_report.txt" \
    "$SCRIPT_DIR/apps/leaky_app" --leak \
    > "$LOG_DIR/valgrind_app_output.txt" 2>&1
  valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all \
    --track-origins=yes --error-exitcode=99 \
    --log-file="$LOG_DIR/valgrind_fixed.txt" \
    "$SCRIPT_DIR/apps/leaky_app_fixed" \
    > "$LOG_DIR/valgrind_fixed_app_output.txt" 2>&1
  if ! grep -q 'All heap blocks were freed' "$LOG_DIR/valgrind_fixed.txt"; then
    echo "Valgrind fixed run did not confirm that all heap blocks were freed." >&2
    exit 1
  fi
  if ! grep -q 'ERROR SUMMARY: 0 errors from 0 contexts' \
    "$LOG_DIR/valgrind_fixed.txt"; then
    echo "Valgrind fixed run reported errors." >&2
    exit 1
  fi
else
  if [[ -s "$LOG_DIR/valgrind_report.txt" && -s "$LOG_DIR/valgrind_fixed.txt" ]]; then
    printf 'Host Valgrind is not installed; existing x86-64 results were collected in the Ubuntu 22.04 toolchain container.\n' |
      tee "$LOG_DIR/valgrind_status.txt"
  else
    printf 'NOT RUN: valgrind is not installed on this host.\n' |
      tee "$LOG_DIR/valgrind_status.txt"
  fi
fi

if command -v strace >/dev/null 2>&1; then
  strace -c -o "$LOG_DIR/strace_summary.txt" \
    "$SCRIPT_DIR/apps/perf_target" \
    > "$LOG_DIR/strace_app_output.txt" 2>&1
  strace -o "$LOG_DIR/strace_full.txt" \
    "$SCRIPT_DIR/apps/perf_target" \
    > "$LOG_DIR/strace_full_app_output.txt" 2>&1
else
  printf 'NOT RUN: strace is not installed on this host.\n' |
    tee "$LOG_DIR/strace_status.txt"
fi

if command -v perf >/dev/null 2>&1; then
  if perf stat -o "$LOG_DIR/perf_stat.txt" \
    "$SCRIPT_DIR/apps/perf_target" \
    > "$LOG_DIR/perf_stat_app_output.txt" 2>&1; then
    if grep -Eq '<not supported>|not counted|not supported' \
      "$LOG_DIR/perf_stat.txt" "$LOG_DIR/perf_stat_app_output.txt"; then
      printf 'perf stat ran, but one or more requested counters were unavailable.\n' |
        tee "$LOG_DIR/perf_status.txt"
    else
      echo "perf stat completed."
    fi
  else
    printf 'perf stat unavailable or denied; see perf_stat_app_output.txt.\n' |
      tee "$LOG_DIR/perf_status.txt"
  fi
  if perf record -g -o "$LOG_DIR/perf.data" \
    -- "$SCRIPT_DIR/apps/perf_target" \
    > "$LOG_DIR/perf_record_app_output.txt" 2>&1; then
    if perf report -i "$LOG_DIR/perf.data" --stdio \
      > "$LOG_DIR/perf_report.txt" 2>> "$LOG_DIR/perf_record_app_output.txt"; then
      echo "perf record/report completed."
    else
      echo "perf report could not decode the recording." |
        tee "$LOG_DIR/perf_status.txt"
      printf 'NOT AVAILABLE: perf report could not decode a recording.\n' \
        > "$LOG_DIR/perf_report.txt"
    fi
  else
    printf 'perf record unavailable or denied; see perf_record_app_output.txt.\n' |
      tee "$LOG_DIR/perf_status.txt"
    if [[ -f "$LOG_DIR/perf.data" && ! -s "$LOG_DIR/perf.data" ]]; then
      rm -f -- "$LOG_DIR/perf.data"
    fi
    printf 'NOT AVAILABLE: perf record could not collect samples on this host.\n' \
      > "$LOG_DIR/perf_report.txt"
  fi
else
  printf 'NOT RUN: perf is not installed on this host.\n' |
    tee "$LOG_DIR/perf_status.txt"
  printf 'NOT RUN: perf is not installed on this host.\n' \
    > "$LOG_DIR/perf_report.txt"
fi

printf 'Host analysis finished; review logs under %s.\n' "$LOG_DIR"
