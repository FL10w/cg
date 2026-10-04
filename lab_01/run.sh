#!/usr/bin/env bash
set -euo pipefail
LAB_ROOT="$(cd "$(dirname "$0")" && pwd)"
if [ -d "$LAB_ROOT/.local/sysroot" ]; then source "$LAB_ROOT/tools/activate.sh"; fi
exec "$LAB_ROOT/build-debug/cg-lab-01" "$@"
