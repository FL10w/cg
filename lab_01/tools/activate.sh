# Source this file from bash/zsh: source tools/activate.sh
if [ -n "${ZSH_VERSION:-}" ]; then
    LAB_ROOT="$(cd "$(dirname "${(%):-%x}")/.." && pwd)"
else
    LAB_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fi
export PATH="$LAB_ROOT/.local/sysroot/usr/bin:$PATH"
export LD_LIBRARY_PATH="$LAB_ROOT/.local/sysroot/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export VK_LAYER_PATH="$LAB_ROOT/.local/sysroot/usr/share/vulkan/explicit_layer.d"
