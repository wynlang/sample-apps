#!/usr/bin/env bash
# Compiles the C shim into libwynimg/libwynimg.a.
# A static archive is required: Wyn's [ffi] table accepts only
# libs / lib_dirs / include_dirs -- there is no `objects` key.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p libwynimg build

CFLAGS="-std=c11 -O2 -Wall -Wextra -Icsrc"
PNG_CFLAGS="$(pkg-config --cflags libpng 2>/dev/null || true)"

OBJS=()
for src in csrc/*.c; do
    obj="build/$(basename "${src%.c}").o"
    cc -c $CFLAGS $PNG_CFLAGS "$src" -o "$obj"
    OBJS+=("$obj")
done

rm -f libwynimg/libwynimg.a
ar rcs libwynimg/libwynimg.a "${OBJS[@]}"
echo "built libwynimg/libwynimg.a ($(wc -c < libwynimg/libwynimg.a) bytes)"
