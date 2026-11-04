#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
mkdir -p "$HOME/embedded_lab2"
if ! podman image exists lab2-build; then
  echo "Build image 'lab2-build' is missing. Run ./00_setup.sh first." >&2
  exit 1
fi

TTY=()
if [[ -t 0 ]]; then
  TTY=(-it)
fi

exec podman run --rm "${TTY[@]}" --userns=keep-id --security-opt label=disable \
  -e HOME="$HOME" \
  -v "$HOME/embedded_lab1:$HOME/embedded_lab1" \
  -v "$HOME/embedded_lab2:$HOME/embedded_lab2" \
  -v "$SCRIPT_DIR:$SCRIPT_DIR:rw" \
  -v "$SCRIPT_DIR:/scripts:ro" \
  -w "$SCRIPT_DIR" \
  lab2-build "$@"
