# Source from bash/zsh; prefer this lab's SDK, then lab 2 and lab 1.
if [ -n "${ZSH_VERSION:-}" ]; then
    LAB_ROOT="$(cd "$(dirname "${(%):-%x}")/.." && pwd)"
else
    LAB_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fi
LAB_DEPENDENCIES="$LAB_ROOT/.local"
for previous_lab in lab2 lab_01; do
    if [ ! -d "$LAB_DEPENDENCIES" ] && [ -d "$LAB_ROOT/../$previous_lab/.local" ]; then
        LAB_DEPENDENCIES="$LAB_ROOT/../$previous_lab/.local"
    fi
done
if [ -d "$LAB_DEPENDENCIES/sysroot" ]; then
    export PATH="$LAB_DEPENDENCIES/sysroot/usr/bin:$PATH"
    export LD_LIBRARY_PATH="$LAB_DEPENDENCIES/sysroot/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export VK_LAYER_PATH="$LAB_DEPENDENCIES/sysroot/usr/share/vulkan/explicit_layer.d"
fi
