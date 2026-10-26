#!/bin/sh
# Print an arm-none-eabi-g++ that can include <cstdint>.
# Homebrew's GCC 16 bottle is built --without-headers.

try_compiler() {
    compiler=$1
    if [ ! -x "$compiler" ]; then
        return 1
    fi
    printf '%s\n' '#include <cstdint>' 'int main(){return 0;}' |
        "$compiler" -std=c++20 -fsyntax-only -xc++ - >/dev/null 2>&1
}

if [ -n "${ARM_CXX:-}" ] && try_compiler "$ARM_CXX"; then
    printf '%s\n' "$ARM_CXX"
    exit 0
fi

if try_compiler "$(command -v arm-none-eabi-g++ 2>/dev/null)"; then
    command -v arm-none-eabi-g++
    exit 0
fi

for compiler in /Applications/STM32CubeIDE.app/Contents/Eclipse/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32*/tools/bin/arm-none-eabi-g++ \
    "$HOME"/workspace/netkit/boards/nucleo-f446re/.toolchain/xpack-arm-none-eabi-gcc-*/bin/arm-none-eabi-g++; do
    if try_compiler "$compiler"; then
        printf '%s\n' "$compiler"
        exit 0
    fi
done

echo "no arm-none-eabi-g++ with C++ headers found" >&2
exit 1
