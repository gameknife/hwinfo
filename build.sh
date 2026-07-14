#!/usr/bin/env sh

set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD_DIR="$ROOT_DIR/build"

if ! command -v cmake >/dev/null 2>&1; then
    echo "Error: cmake was not found in PATH." >&2
    exit 1
fi

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --config Release --parallel

echo ""
echo "Build complete:"
echo "  $BUILD_DIR/bin/system_info"
echo "  $BUILD_DIR/bin/hardware_summary"
