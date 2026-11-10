#!/usr/bin/env bash
set -euo pipefail
LAB_ROOT="$(cd "$(dirname "$0")" && pwd)"
source "$LAB_ROOT/tools/activate.sh"
cmake -S "$LAB_ROOT" -B "$LAB_ROOT/build-debug" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$LAB_ROOT/build-debug" --parallel 4
ctest --test-dir "$LAB_ROOT/build-debug" --output-on-failure
