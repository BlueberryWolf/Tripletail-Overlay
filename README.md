# Tripletail Overlay

Low latency, audio visualizer and desktop overlay for [Tripletail FM](https://tripletail.blueberry.coffee).

## Features
- Bouncy tail that moves with the bass
- Stays on top and skips the taskbar (win32/x11)
- Right click to cycle modes: auto, pinned, locked
- Scroll wheel adjusts volume and saves it to a file

## Building

<details>
<summary><b>Windows (MSYS2)</b></summary>

1. Install [MSYS2](https://www.msys2.org/).
2. Open a **MinGW64** terminal and install the required packages:
   ```bash
   pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-curl mingw-w64-x86_64-opusfile mingw-w64-x86_64-raylib pkg-config
   ```
3. Run `.\build.bat` in the project root.
</details>

<details>
<summary><b>macOS (Homebrew)</b></summary>

1. Install [Homebrew](https://brew.sh/).
2. Install the required packages:
   ```bash
   brew install raylib opusfile curl pkg-config
   ```
3. Run `chmod +x build.sh && ./build.sh`.
</details>

<details>
<summary><b>Linux (Ubuntu)</b></summary>

1. Add the Raylib PPA and install dependencies:
   ```bash
   sudo add-apt-repository ppa:texus/raylib
   sudo apt update
   sudo apt install build-essential libraylib6-dev libcurl4-openssl-dev libopusfile-dev libx11-dev libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev libgl1-mesa-dev
   ```
2. Run `chmod +x build.sh && ./build.sh`.
</details>

<details>
<summary><b>Linux (Arch)</b></summary>

1. Install the required packages:
   ```bash
   sudo pacman -S --needed base-devel raylib curl opusfile libx11 libxcursor libxinerama libxrandr libxi mesa pkgconf
   ```
2. Run `chmod +x build.sh && ./build.sh`.
</details>

## Usage
- Right click to change visibility
- Scroll to change volume
- Hover the tail to see the track info
