#!/bin/bash

# pkg-config is the way
CFLAGS="-Wall -O3 -ffast-math -flto -Dkiss_fft_scalar=float -I./deps/raylib/include -I./deps/kiss_fft -I./deps/stb -I./src -I/usr/include/opus $(pkg-config --cflags raylib opusfile curl)"
LDFLAGS="-flto -Wl,--gc-sections $(pkg-config --libs raylib opusfile curl || echo -lraylib -lopusfile -lcurl) -lX11 -lpthread -lm -ldl -lrt"

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
gcc $CFLAGS -c src/platform.c -o build/platform.o

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
fi
