#!/bin/bash
set -e

# base flags
CFLAGS="-Wall -O3 -ffast-math -flto -Dkiss_fft_scalar=float -I./deps/raylib/include -I./deps/kiss_fft -I./deps/stb -I./src"
LDFLAGS="-flto"

# detect os
OS_NAME=$(uname -s)
echo "Building for $OS_NAME..."

if [ "$OS_NAME" == "Darwin" ]; then
    CFLAGS="$CFLAGS $(pkg-config --cflags raylib opusfile libcurl)"
    CFLAGS="$CFLAGS -I$(pkg-config --variable=includedir opusfile | sed 's/\/opus$//')"
    RAYLIB_STATIC=$(pkg-config --libs --static raylib 2>/dev/null || echo "-lraylib")
    OPUS_STATIC=$(pkg-config --libs --static opusfile 2>/dev/null || echo "-lopusfile -lopus -logg")
    LDFLAGS="$LDFLAGS -Wl,-dead_strip $RAYLIB_STATIC $OPUS_STATIC -lcurl -framework AppKit -framework CoreGraphics -framework IOKit -framework AudioToolbox -framework CoreVideo -framework Cocoa"
    PLATFORM_SRC="src/platform_macos.m"
else
    CFLAGS="$CFLAGS -I/usr/include/opus $(pkg-config --cflags raylib opusfile libcurl)"
    RAYLIB_STATIC=$(pkg-config --libs --static raylib 2>/dev/null || echo "-lraylib")
    OTHER_LIBS=$(pkg-config --libs opusfile libcurl || echo "-lopusfile -lcurl")
    LDFLAGS="$LDFLAGS -Wl,--gc-sections $RAYLIB_STATIC $OTHER_LIBS -lX11 -lXcursor -lXinerama -lXi -lXrandr -lGL -lpthread -lm -ldl -lrt"
    PLATFORM_SRC="src/platform_linux.c"
fi

mkdir -p build

echo "Compiling kiss_fft..."
gcc $CFLAGS -c deps/kiss_fft/kiss_fft.c -o build/kiss_fft.o

echo "Compiling network..."
gcc $CFLAGS -c src/network.c -o build/network.o

echo "Compiling ringbuffer..."
gcc $CFLAGS -c src/ringbuffer.c -o build/ringbuffer.o

echo "Compiling stb_impl..."
gcc $CFLAGS -c deps/stb/stb_impl.c -o build/stb_impl.o

echo "Compiling platform..."
if [[ "$PLATFORM_SRC" == *.m ]]; then
    clang $CFLAGS -c $PLATFORM_SRC -o build/platform.o
else
    gcc $CFLAGS -c $PLATFORM_SRC -o build/platform.o
fi

echo "Compiling audio..."
gcc $CFLAGS -c src/audio.c -o build/audio.o

echo "Compiling ui..."
gcc $CFLAGS -c src/ui.c -o build/ui.o

echo "Compiling main..."
gcc $CFLAGS -c src/main.c -o build/main.o

echo "Linking..."
gcc build/*.o $LDFLAGS -o tripletail-overlay

if [ $? -eq 0 ]; then
    echo "Build successful: tripletail-overlay"
else
    echo "Build failed. go check the errors lol"
    exit 1
fi