#!/usr/bin/env bash
set -euo pipefail
LAB_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$LAB_ROOT"
source tools/activate.sh
export LAB_REQUIRE_VALIDATION=1 LAB_SYNC_VALIDATION=1
mkdir -p checks/screenshots
ctest --test-dir build-debug --output-on-failure > checks/math.log
capture() {
    local name="$1"
    shift
    xvfb-run -a ./run.sh --frames 35 "$@" --capture "checks/screenshots/$name.ppm" > "checks/$name.log" 2>&1
    cat "checks/$name.log"
}
capture default --resize-test
capture side --view side
capture wide --view wide
for preset in ambient diffuse specular red-light matte; do
    capture "$preset" --preset "$preset"
done
python3 tools/check_render.py
