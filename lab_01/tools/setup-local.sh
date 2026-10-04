#!/usr/bin/env bash
set -euo pipefail
LAB_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$LAB_ROOT/.local/packages"
cd "$LAB_ROOT/.local/packages"
# Debian/Kali/Ubuntu only; package versions follow the machine's apt repositories.
apt download libvulkan-dev glslc libshaderc1 vulkan-validationlayers libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxfixes-dev libxrender-dev libxrandr2 libxinerama1 libxcursor1 libxi6 libxfixes3 libxrender1
for package in ./*.deb; do dpkg-deb -x "$package" "$LAB_ROOT/.local/sysroot"; done
# The loader already installed with the GPU driver supplies libvulkan.so.1.
LAB_LOADER="$(ldconfig -p | awk '/libvulkan.so.1 .*x86-64/ {print $NF; exit}')"
if [ -z "$LAB_LOADER" ]; then
    echo 'Install a Vulkan-capable driver (or mesa-vulkan-drivers) first.' >&2
    exit 1
fi
ln -sfn "$LAB_LOADER" "$LAB_ROOT/.local/sysroot/usr/lib/x86_64-linux-gnu/libvulkan.so"
printf 'Ready. Run: source "%s/tools/activate.sh"\n' "$LAB_ROOT"
