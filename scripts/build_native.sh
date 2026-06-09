#!/usr/bin/env bash
# Build native FFI library and standalone server for the host platform.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${ROOT}/native/build"
BUILD_TYPE="${BUILD_TYPE:-Release}"

cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5

cmake --build "${BUILD_DIR}" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

echo ""
echo "Built:"
echo "  Shared lib : ${BUILD_DIR}/libflutter_datachannel.so (or .dylib / .dll)"
echo "  Server bin : ${BUILD_DIR}/fdc-server"
