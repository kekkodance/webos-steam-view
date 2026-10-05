#!/usr/bin/env bash
# One-time setup for host builds (Windows/MinGW). The webOS cross build
# doesn't need this (Linux CI uses system packages).
set -euo pipefail
cd "$(dirname "$0")/.."

mkdir -p third_party/mbedtls-tar
[ -f third_party/mbedtls-tar/mbedtls.tar.gz ] || \
    curl -sL -o third_party/mbedtls-tar/mbedtls.tar.gz \
        https://github.com/Mbed-TLS/mbedtls/archive/refs/tags/v3.4.0.tar.gz

# protobuf-c runtime with the include layout ihslib expects (<protobuf-c/protobuf-c.h>)
mkdir -p third_party/protobuf-c-out/protobuf-c
cp third_party/protobuf-c/protobuf-c/protobuf-c.c third_party/protobuf-c-out/protobuf-c/
cp third_party/protobuf-c/protobuf-c/protobuf-c.h third_party/protobuf-c-out/protobuf-c/

# SDL2 + SDL2_net for the Windows host build
if [ ! -f third_party/SDL-webOS/build/SDL2.dll ]; then
    cmake -S third_party/SDL-webOS -B third_party/SDL-webOS/build \
        -DCMAKE_BUILD_TYPE=Release \
        -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF \
        -DSDL_RENDER=ON -DSDL_VIDEO=ON -DSDL_AUDIO=OFF -DSDL_JOYSTICK=OFF \
        -DSDL_HAPTIC=OFF -DSDL_HIDAPI=OFF -DSDL_POWER=OFF -DSDL_TIMERS=ON \
        -DSDL_FILE=ON -DSDL_LOADSO=ON -DSDL_CPUINFO=OFF -DSDL_FILESYSTEM=OFF \
        -DSDL_SENSOR=OFF -DSDL_THREADS=ON -DSDL_LOCALE=OFF -DSDL_MISC=OFF
    cmake --build third_party/SDL-webOS/build -j"$(nproc 2>/dev/null || echo 8)"
fi

# headers in SDL2/ layout for SDL_net's find module
mkdir -p third_party/sdl2-pc/SDL2
cp third_party/SDL-webOS/include/*.h third_party/sdl2-pc/SDL2/

if [ ! -f third_party/SDL_net/build/SDL2_net.dll ]; then
    ROOT="$(pwd)"
    cmake -S third_party/SDL_net -B third_party/SDL_net/build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH="${ROOT}/third_party/SDL-webOS/build;${ROOT}/third_party/sdl2-pc" \
        -DSDL2_INCLUDE_DIR="${ROOT}/third_party/sdl2-pc/SDL2" \
        -DSDL2_LIBRARY="${ROOT}/third_party/SDL-webOS/build/libSDL2.dll.a"
    cmake --build third_party/SDL_net/build -j"$(nproc 2>/dev/null || echo 8)"
fi

echo "host setup done"
