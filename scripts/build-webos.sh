#!/usr/bin/env bash
# Build the webOS IPK for Steam View.
#
# Requirements:
#   - webOS NDK (openlgtv/buildroot-nc4):
#       https://github.com/openlgtv/buildroot-nc4/releases
#       downloaded, relocated, and TOOLCHAIN_FILE set to its
#       share/buildroot/toolchainfile.cmake
#   - webos-userland headers (lgncopenapi, opus pkg-config files):
#       git clone https://github.com/webosbrew/webos-userland /opt/webos-userland
#   - ares-package (webOS SDK) for IPK packing
set -euo pipefail
cd "$(dirname "$0")/.."

: "${TOOLCHAIN_FILE:?Set TOOLCHAIN_FILE to the buildroot-nc4 toolchainfile.cmake}"

# Ensure ihslib carries our portability patches (idempotent)
"$(dirname "$0")/apply-ihslib-patches.sh"

BUILD_DIR=build/webos
mkdir -p "${BUILD_DIR}"
cmake -B"${BUILD_DIR}" \
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DSTEAMVIEW_BUILD_TESTS=OFF \
    .
cmake --build "${BUILD_DIR}" -j"$(nproc 2>/dev/null || sysctl -n hw.logicalcpu)"

# Pack the IPK (installs into the staging tree + AresPackage fallback)
cmake --build "${BUILD_DIR}" --target webos-package

echo
echo "IPK files:"
ls -la dist/*.ipk 2>/dev/null || { echo "no IPK produced!" >&2; exit 1; }
