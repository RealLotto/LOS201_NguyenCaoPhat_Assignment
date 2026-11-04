#!/bin/bash
# HOST. Runs a command inside the Ubuntu toolchain container.
# ~/embedded_lab1 and ~/embedded_lab2 are mounted at the SAME absolute paths, so KDIR etc. keep working.
D="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HOME/embedded_lab1" "$HOME/embedded_lab2"
TTY=-i; [ -t 0 ] && TTY=-it
exec podman run --rm $TTY --userns=keep-id --security-opt label=disable \
  -e HOME="$HOME" -v "$HOME/embedded_lab1:$HOME/embedded_lab1" -v "$HOME/embedded_lab2:$HOME/embedded_lab2" \
  -v "$D:/scripts:ro" -w "$HOME/embedded_lab1" lab1-build "$@"
