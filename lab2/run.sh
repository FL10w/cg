#!/usr/bin/env bash
set -euo pipefail
LAB_ROOT="$(cd "$(dirname "$0")" && pwd)"
source "$LAB_ROOT/tools/activate.sh"
exec "$LAB_ROOT/build-debug/cg-lab-02" "$@"
