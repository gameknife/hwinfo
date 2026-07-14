#!/usr/bin/env sh

set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD_DIR="$ROOT_DIR/build"
GPU_CATALOG="$ROOT_DIR/data/local/passmark_gpu_catalog.csv"

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        OUTPUT_EXE="$BUILD_DIR/bin/hardware_requirements_embedded.exe"
        ;;
    *)
        OUTPUT_EXE="$BUILD_DIR/bin/hardware_requirements_embedded"
        ;;
esac

if ! command -v cmake >/dev/null 2>&1; then
    echo "Error: cmake was not found in PATH." >&2
    exit 1
fi

if [ ! -f "$GPU_CATALOG" ]; then
    echo "Error: local GPU catalog was not found:" >&2
    echo "  $GPU_CATALOG" >&2
    echo "Run tools/update_passmark_gpu_catalog.py first." >&2
    exit 1
fi

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
    -DHWINFO_BUILD_LOCAL_REQUIREMENTS=ON \
    -DHWINFO_LOCAL_GPU_CATALOG="$GPU_CATALOG" \
    -DHWINFO_LOCAL_MIN_CPU_CORES=4 \
    -DHWINFO_LOCAL_MIN_GPU_MODEL="GeForce RTX 3080 Ti" \
    -DHWINFO_LOCAL_MIN_MEMORY_GIB=8 \
    -DHWINFO_LOCAL_REQUIRE_SSD=ON

cmake --build "$BUILD_DIR" --config Release --target hardware_requirements_embedded --parallel

if [ ! -f "$OUTPUT_EXE" ]; then
    echo "Error: build completed but the expected executable was not found:" >&2
    echo "  $OUTPUT_EXE" >&2
    exit 1
fi

echo ""
echo "Build complete:"
echo "  $OUTPUT_EXE"
