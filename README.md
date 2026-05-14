# Tripletail Overlay

Low latency, audio visualizer and desktop overlay for [Tripletail FM](https://tripletail.blueberry.coffee).

## Features
- Bouncy tail that moves with the bass
- Stays on top and skips the taskbar (win32/x11)
- Right click to cycle modes: auto, pinned, locked
- Scroll wheel adjusts volume and saves it to a file

## Building

### 1. Get dependencies
- **Windows**: Install [MSYS2](https://www.msys2.org/) and run this in a **MinGW64** shell:
  ```bash
  pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-curl mingw-w64-x86_64-opusfile pkg-config
  ```
- **Linux (Debian/Ubuntu)**: Run:
  ```bash
  sudo apt install build-essential libcurl4-openssl-dev libopusfile-dev libx11-dev libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev libgl1-mesa-dev
  ```
- **Linux (Arch)**: Run:
  ```bash
  sudo pacman -S --needed base-devel curl opusfile libx11 libxcursor libxinerama libxrandr libxi mesa pkgconf
  ```

### 2. Build
- **Windows**: run `.\build.bat`.
- **Linux**: run `chmod +x build.sh && ./build.sh`. 

## Usage
- Right click to change visibility
- Scroll to change volume
- Hover the tail to see the track info
