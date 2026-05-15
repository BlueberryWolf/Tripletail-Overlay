@echo off
setlocal enabledelayedexpansion

:: kill it if it's running so we can actually link
taskkill /f /im tripletail-overlay.exe >nul 2>nul

:: base flags
set CFLAGS=-Wall -O3 -ffast-math -flto -Dkiss_fft_scalar=float -DCURL_STATICLIB -ffunction-sections -fdata-sections -I.\deps\raylib\include -I.\deps\kiss_fft -I.\deps\stb -I.\src
set LDFLAGS=-L.\deps\raylib\lib -flto -Wl,--gc-sections -s -mwindows -lraylib -lgdi32 -lwinmm -lole32 -loleaut32 -luuid -lpsapi -lm

:: detect dependencies
where pkg-config >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    pkg-config --exists opusfile libcurl >nul 2>nul
    if !ERRORLEVEL! EQU 0 (
        for /f "tokens=*" %%i in ('pkg-config --cflags opusfile libcurl') do set CFLAGS=%CFLAGS% %%i
        for /f "tokens=*" %%i in ('pkg-config --static --libs opusfile libcurl') do set LDFLAGS=%LDFLAGS% %%i
        goto :START_BUILD
    )
)

:: fallback garbage
set LDFLAGS=%LDFLAGS% -lcurl -lopusfile -lopus -logg
if exist "C:\msys64\mingw64\include\opus" (
    set CFLAGS=%CFLAGS% -IC:\msys64\mingw64\include\opus
) else if defined MSYSTEM (
    set CFLAGS=%CFLAGS% -I/mingw64/include/opus
)

:START_BUILD
if exist build rd /s /q build
if not exist build mkdir build

echo Compiling KissFFT, STB, and Core Source...
gcc %CFLAGS% -c deps\kiss_fft\kiss_fft.c -o build\kiss_fft.o
gcc %CFLAGS% -c deps\stb\stb_impl.c -o build\stb_impl.o

for %%s in (network ringbuffer audio ui main) do (
    echo compiling %%s...
    gcc %CFLAGS% -c src\%%s.c -o build\%%s.o
)

echo Compiling platform...
gcc %CFLAGS% -c src\platform_win32.c -o build\platform.o

echo Linking...
gcc build\*.o %LDFLAGS% -o tripletail-overlay.exe

if %ERRORLEVEL% EQU 0 (
    echo Build successful: tripletail-overlay.exe
) else (
    echo Build failed.
    exit /b 1
)