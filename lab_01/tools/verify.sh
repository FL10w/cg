#!/usr/bin/env bash
set -euo pipefail
LAB_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$LAB_ROOT"
source tools/activate.sh
export LAB_REQUIRE_VALIDATION=1 LAB_SYNC_VALIDATION=1
mkdir -p report/screenshots report/checks
ctest --test-dir build-debug --output-on-failure > report/checks/math.log
for view in default side; do
    extra=()
    if [ "$view" = default ]; then extra+=(--resize-test); fi
    xvfb-run -a ./run.sh --frames 60 --view "$view" "${extra[@]}" --capture "report/screenshots/$view.ppm" > "report/checks/$view.log" 2>&1
    cat "report/checks/$view.log"
done
python3 tools/check_render.py
