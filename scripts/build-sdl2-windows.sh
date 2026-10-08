#!/bin/sh
# Builds a minimal static SDL2 for the Windows build of COrg.
#
# Only the parts of SDL that COrg uses (audio, threads, timers) are
# built, and sdl2-minimal.patch removes the remaining references from
# audio and assertions to video and events, so that static linking can
# leave the rest out. The result goes in PREFIX, ready for:
#
#     make windows SDL2_MINGW=PREFIX
#
# Usage: scripts/build-sdl2-windows.sh PREFIX
#
# Requires curl, sha256sum, tar, patch, cmake and a MinGW-w64 gcc
# (x86_64-w64-mingw32-gcc by default; set CC and RC to override).

set -eu

VERSION=2.32.10
SHA256=5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165
URL=https://github.com/libsdl-org/SDL/releases/download/release-$VERSION/SDL2-$VERSION.tar.gz

CC=${CC:-x86_64-w64-mingw32-gcc}
RC=${RC:-x86_64-w64-mingw32-windres}

if [ $# -ne 1 ]; then
    echo "Usage: $0 PREFIX" >&2
    exit 1
fi

script_dir=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$1"
prefix=$(cd "$1" && pwd)
work=$prefix/build

if [ -e "$work" ]; then
    echo "$work already exists; remove it to rebuild." >&2
    exit 1
fi
mkdir "$work"

curl -fL -o "$work/SDL2-$VERSION.tar.gz" "$URL"
echo "$SHA256  $work/SDL2-$VERSION.tar.gz" | sha256sum -c -
tar xzf "$work/SDL2-$VERSION.tar.gz" -C "$work"
patch -d "$work/SDL2-$VERSION" -p1 < "$script_dir/sdl2-minimal.patch"

cmake -S "$work/SDL2-$VERSION" -B "$work/cmake" \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER="$CC" \
    -DCMAKE_RC_COMPILER="$RC" \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DCMAKE_C_FLAGS="-ffunction-sections -fdata-sections" \
    -DCMAKE_INSTALL_PREFIX="$prefix" \
    -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF \
    -DSDL_VIDEO=OFF -DSDL_RENDER=OFF -DSDL_EVENTS=OFF \
    -DSDL_JOYSTICK=OFF -DSDL_HAPTIC=OFF -DSDL_HIDAPI=OFF \
    -DSDL_POWER=OFF -DSDL_SENSOR=OFF -DSDL_LOCALE=OFF \
    -DSDL_MISC=OFF -DSDL_FILESYSTEM=OFF
cmake --build "$work/cmake" --parallel
cmake --install "$work/cmake"

echo
echo "Done. Build COrg with: make windows SDL2_MINGW=$prefix"
