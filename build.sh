#!/bin/bash
set -e

# base flags
CFLAGS="-Wall -O3 -ffast-math -flto -Dkiss_fft_scalar=float -I./deps/raylib/include -I./deps/kiss_fft -I./deps/stb -I./src"
LDFLAGS="-flto"

# detect os
OS_NAME=$(uname -s)
echo "Building for $OS_NAME..."

# setup platform dependencies
if [ "$OS_NAME" == "Darwin" ]; then
    URL="https://github.com/raysan5/raylib/releases/download/6.0/raylib-6.0_macos.tar.gz"
    curl $URL -L -O && tar -xzf raylib-6.0_macos.tar.gz

    CFLAGS="$CFLAGS $(pkg-config --cflags raylib opusfile libcurl)"
    CFLAGS="$CFLAGS -I$(pkg-config --variable=includedir opusfile | sed 's/\/opus$//')"
    
    RAYLIB_STATIC="./raylib-6.0_macos/lib/libraylib.a"
    OPUSFILE_LIBDIR=$(pkg-config --variable=libdir opusfile 2>/dev/null || echo "/usr/local/lib")
    OPUS_LIBDIR=$(pkg-config --variable=libdir opus 2>/dev/null || echo "/usr/local/lib")
    OGG_LIBDIR=$(pkg-config --variable=libdir ogg 2>/dev/null || echo "/usr/local/lib")
    
    OPUS_STATIC="$OPUSFILE_LIBDIR/libopusfile.a $OPUS_LIBDIR/libopus.a $OGG_LIBDIR/libogg.a"
    LDFLAGS="$LDFLAGS -Wl,-dead_strip $RAYLIB_STATIC $OPUS_STATIC -lcurl -framework AppKit -framework CoreGraphics -framework IOKit -framework AudioToolbox -framework CoreVideo -framework Cocoa"
    PLATFORM_SRC="src/platform_macos.m"
else
    URL="https://github.com/raysan5/raylib/releases/download/6.0/raylib-6.0_linux_amd64.tar.gz"
    curl $URL -L -O && tar -xzf raylib-6.0_linux_amd64.tar.gz

    CFLAGS="$CFLAGS -I/usr/include/opus $(pkg-config --cflags raylib opusfile libcurl)"
    
    RAYLIB_STATIC="./raylib-6.0_linux_amd64/lib/libraylib.a"
    OPUSFILE_LIBDIR=$(pkg-config --variable=libdir opusfile 2>/dev/null || echo "/usr/lib/x86_64-linux-gnu")
    OPUS_LIBDIR=$(pkg-config --variable=libdir opus 2>/dev/null || echo "/usr/lib/x86_64-linux-gnu")
    OGG_LIBDIR=$(pkg-config --variable=libdir ogg 2>/dev/null || echo "/usr/lib/x86_64-linux-gnu")
    
    OPUS_STATIC="$OPUSFILE_LIBDIR/libopusfile.a $OPUS_LIBDIR/libopus.a $OGG_LIBDIR/libogg.a"
    LDFLAGS="$LDFLAGS -Wl,--gc-sections $RAYLIB_STATIC $OPUS_STATIC -lcurl -lX11 -lXcursor -lXinerama -lXi -lXrandr -lGL -lpthread -lm -ldl -lrt"
    PLATFORM_SRC="src/platform_linux.c"
fi

mkdir -p build

# compile objects
echo "Compiling KissFFT, STB, and Core Source..."
gcc $CFLAGS -c deps/kiss_fft/kiss_fft.c -o build/kiss_fft.o
gcc $CFLAGS -c deps/stb/stb_impl.c -o build/stb_impl.o

for src in network ringbuffer audio ui main; do
    echo "Compiling $src..."
    gcc $CFLAGS -c src/$src.c -o build/$src.o
done

echo "Compiling platform..."
if [[ "$PLATFORM_SRC" == *.m ]]; then
    clang $CFLAGS -c $PLATFORM_SRC -o build/platform.o
else
    gcc $CFLAGS -c $PLATFORM_SRC -o build/platform.o
fi

# link
echo "Linking..."
gcc build/*.o $LDFLAGS -o tripletail-overlay

if [ $? -eq 0 ]; then
    echo "Build successful: tripletail-overlay"
else
    echo "Build failed. go check the errors lol"
    exit 1
fi