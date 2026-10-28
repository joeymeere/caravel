#!/usr/bin/env bash
#
# SBF compile matrix: build compile/probe.c for every combination of SBPF
# version, C standard, entrypoint style and compile option with -Werror.
#
# Uses SOLANA_CLANG if set, otherwise the newest platform-tools in
# ~/.cache/solana (same lookup as `caravel build`).

set -uo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
inc="$here/../../include"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT

clang="${SOLANA_CLANG:-}"
if [ -z "$clang" ]; then
    clang="$(ls -d "$HOME"/.cache/solana/v*/platform-tools/llvm/bin/clang 2>/dev/null \
             | sort -V | tail -1)"
fi
if [ -z "$clang" ] || [ ! -x "$clang" ]; then
    echo "matrix: platform-tools clang not found (set SOLANA_CLANG)" >&2
    exit 1
fi

cpus=("generic" "v3")
stds=("c99" "c11" "c17" "gnu11")
entries=("" "-DPROBE_LEGACY")
opts=(
    ""
    "-DNO_HEAP"
    "-DCUSTOM_HEAP"
    "-DNO_TOKEN -DNO_SYSTEM"
    "-DALLOW_DUPLICATE_ACCOUNTS -DDEBUG"
    "-DMAX_ACCOUNTS=32 -DHEAP_SZ=65536"
)

total=0
failed=0
for cpu in "${cpus[@]}"; do
    extra=""
    [ "$cpu" = "v3" ] && extra="-DCVL_STATIC_SYSCALLS"
    for std in "${stds[@]}"; do
        for entry in "${entries[@]}"; do
            for opt in "${opts[@]}"; do
                total=$((total + 1))
                # shellcheck disable=SC2086
                if ! log="$("$clang" --target=sbf -mcpu="$cpu" -std="$std" -O2 \
                        -fno-builtin -Wall -Wextra -Wpedantic -Werror \
                        -Wno-gnu-statement-expression \
                        -Wno-gnu-zero-variadic-macro-arguments \
                        -Wno-gnu-empty-struct -Wno-language-extension-token \
                        -I"$inc" $extra $entry $opt \
                        -c "$here/probe.c" -o "$out/probe.o" 2>&1)"; then
                    failed=$((failed + 1))
                    echo "FAIL [-mcpu=$cpu -std=$std $entry $opt]"
                    echo "$log" | head -20 | sed 's/^/    /'
                fi
            done
        done
    done
done

echo "compile matrix: $((total - failed))/$total configs passed ($clang)"
[ "$failed" -eq 0 ]
