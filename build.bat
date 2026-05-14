@echo off
setlocal enabledelayedexpansion

:: kill it if it's running so we can actually link
taskkill /f /im tripletail-overlay.exe >nul 2>nul

set CFLAGS=-Wall -O3 -ffast-math -flto -Dkiss_fft_scalar=float -ffunction-sections -fdata-sections -I.\deps\raylib\include -I.\deps\kiss_fft -I.\deps\stb -I.\src
set LDFLAGS=-L.\deps\raylib\lib -flto -Wl,--gc-sections -s -mwindows -lraylib -lgdi32 -lwinmm -lole32 -loleaut32 -luuid -lpsapi -lm

:: check if pkg-config actually works for our packages
set USE_PKG=0
where pkg-config >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    pkg-config --exists opusfile libcurl >nul 2>nul
    if !ERRORLEVEL! EQU 0 set USE_PKG=1
)

if !USE_PKG! EQU 1 (
    for /f "tokens=*" %%i in ('pkg-config --cflags opusfile libcurl') do set PKG_CFLAGS=%%i
    for /f "tokens=*" %%i in ('pkg-config --libs opusfile libcurl') do set PKG_LDFLAGS=%%i
    set CFLAGS=%CFLAGS% !PKG_CFLAGS!
    set LDFLAGS=%LDFLAGS% !PKG_LDFLAGS!
) else (
    :: fallback garbage
    set LDFLAGS=%LDFLAGS% -lcurl -lopusfile -lopus -logg
    if exist "C:\msys64\mingw64\include\opus" (
        set CFLAGS=%CFLAGS% -IC:\msys64\mingw64\include\opus
    ) else if defined MSYSTEM (
        set CFLAGS=%CFLAGS% -I/mingw64/include/opus
    )
)

if not exist build mkdir build

echo Compiling kiss_fft...
gcc %CFLAGS% -c deps\kiss_fft\kiss_fft.c -o build\kiss_fft.o

echo Compiling network...
gcc %CFLAGS% -c src\network.c -o build\network.o

echo Compiling ringbuffer...
gcc %CFLAGS% -c src\ringbuffer.c -o build\ringbuffer.o

echo Compiling stb_impl...
gcc %CFLAGS% -c deps\stb\stb_impl.c -o build\stb_impl.o

echo Compiling platform...
gcc %CFLAGS% -c src\platform_win32.c -o build\platform.o

echo Compiling audio...
gcc %CFLAGS% -c src\audio.c -o build\audio.o

echo Compiling ui...
gcc %CFLAGS% -c src\ui.c -o build\ui.o

echo Compiling main...
gcc %CFLAGS% -c src\main.c -o build\main.o

echo Linking...
gcc build\kiss_fft.o build\network.o build\ringbuffer.o build\stb_impl.o build\platform.o build\audio.o build\ui.o build\main.o %LDFLAGS% -o tripletail-overlay.exe

if %ERRORLEVEL% EQU 0 (
    echo Build successful: tripletail-overlay.exe
) else (
    echo Build failed.
)
