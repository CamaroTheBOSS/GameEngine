@echo off
IF NOT EXIST ..\build (
	mkdir ..\build
)
REM Set up Visual studio env variables
setlocal
REM Adjust these in your environment
set VS_TOOLKIT_VERSION=14.44.35207
set VS_SDK_VERSION=10.0.26100.0
set VS_INSTALL_DIR=C:\Program Files\Microsoft Visual Studio\2022\Community
set VS_SDK_DIR=C:\Program Files (x86)\Windows Kits\10

set VS_TOOLKIT_DIR=%VS_INSTALL_DIR%\VC\Tools\MSVC\%VS_TOOLKIT_VERSION%
set MSVC_TOOLS_PATH=%VS_TOOLKIT_DIR%\bin\Hostx64\x64
set WIN_INCLUDE_FLAGS=-I"%VS_TOOLKIT_DIR%\include"
set WIN_INCLUDE_FLAGS=%WIN_INCLUDE_FLAGS% -I"%VS_SDK_DIR%\Include\%VS_SDK_VERSION%\ucrt"
set WIN_INCLUDE_FLAGS=%WIN_INCLUDE_FLAGS% -I"%VS_SDK_DIR%\Include\%VS_SDK_VERSION%\um"
set WIN_INCLUDE_FLAGS=%WIN_INCLUDE_FLAGS% -I"%VS_SDK_DIR%\Include\%VS_SDK_VERSION%\shared"
set WIN_LIB_FLAGS=-L"%VS_TOOLKIT_DIR%\lib\x64"
set WIN_LIB_FLAGS=%WIN_LIB_FLAGS% -L"%VS_SDK_DIR%\Lib\%VS_SDK_VERSION%\um\x64"
set WIN_LIB_FLAGS=%WIN_LIB_FLAGS% -L"%VS_SDK_DIR%\Lib\%VS_SDK_VERSION%\ucrt\x64"
set PATH=%MSVC_TOOLS_PATH%;%PATH%

set ClangExe= D:\Compilers\LLVM\bin\clang++.exe
REM remove -march=native when macros are introduced to SIMD!!!
set CompilerFlags= -O3 -march=native -fuse-ld=lld -std=c++23 -g -gcodeview -fms-runtime-lib=static_dbg
set CompilerFlags= -DINTERNAL_BUILD=0 -DSLOW_VALIDATION=0 %CompilerFlags%
set LinkerFlags= %WIN_LIB_FLAGS% -l user32 -l gdi32 -l ole32 -l winmm
set DllExports=-Xlinker /export:GameMainLoopFrame -Xlinker /export:GameFillSoundBuffer -Xlinker /export:DebugInit -Xlinker /export:DebugFinishFrame
pushd ..\build
  echo %cd%
  del *.pdb > NUL 2> NUL
  REM %ClangExe% %CompilerFlags% -S ..\code\engine.cpp -- -S for assembly => for optimization
  %ClangExe% %CompilerFlags% ..\code\engine_optimized.cpp -c -o engine_optimized.obj -Wunused-command-line-argument
  %ClangExe% %CompilerFlags% ..\code\engine.cpp engine_optimized.obj -shared -o engine.dll %WIN_LIB_FLAGS% %DllExports% -Xlinker /pdb:engine%random%.pdb 
  %ClangExe% %CompilerFlags% ..\code\win32_main.cpp -o win32_main.exe -Wl,%LinkerFlags%
popd