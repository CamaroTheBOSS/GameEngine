This repository has been moved to Codeberg and set to private. It won't be maintained anymore on github

## Prerequisities

- Windows 10+
- Windows SDK / Visual C++ (installed with Visual Studio)
- MSVC or clang++

## Build

Open Visual Studio Developer Command Prompt to set neccessary environment variables or modify build script and set the variables yourself (listed below are default):
- VCToolsVersion=14.44.35207
- WindowsSDKVersion=10.0.26100.0
- VCINSTALLDIR=C:\Program Files\Microsoft Visual Studio\2022\Community\VC
- WindowsSdkDir=C:\Program Files (x86)\Windows Kits\10

From repository root:

To build with MSVC:
```
cd ./code
./build.bat
```

To build with clang++:
```
cd ./code
./clang_build.bat
```

## Run

From repository root:

Generate placeholder asset files and run the game:
```
cd ./data
../build/tools_asset_file_composer.exe
../build/win32_main.exe
```

## Features

- Custom software renderer optimized with SIMD to handle each pixel in 19cycles (intel i5 10400F)
- Hardware OpenGL-based renderer (software/hardware renderers may be switched at runtime)
- Custom asset file format for fast asset loading
- Asset streaming system working based on hand-written general purpose allocator with limited amount of cache memory and LRU-style asset replacement if memory is over
- Audio chunk-based prefetcher and SIMD-optimized audio renderer, which supports smooth volume changes and pitch scaling
- Event-based CPU profiler with debug overlay showing most expensive functions executed on last few frames and call hierarchy with respect to the thread where it was executed
- Dynamic code reloads after each compilation
- Input looping used for fast game development and reproducing/fixing bugs



