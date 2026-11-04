#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
"$SCRIPT_DIR/00_environment.sh"
exec "$SCRIPT_DIR/container_run.sh" bash /scripts/04_make_jffs2_fixture_inner.sh
