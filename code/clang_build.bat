@echo off
IF NOT EXIST ..\build (
	mkdir ..\build
)



REM Set up Visual studio env variables
REM Use VS developer command prompt or adjust these in your environment
REM Variables mirrors what Developer command prompt sets
setlocal
IF DEFINED VCToolsVersion GOTO :VS_VARIABLES_ARE_SET_UP
set VCToolsVersion=14.44.35207
set WindowsSDKVersion=10.0.26100.0
set VCINSTALLDIR=C:\Program Files\Microsoft Visual Studio\2022\Community\VC
set WindowsSdkDir=C:\Program Files (x86)\Windows Kits\10
set VCToolsInstallDir=%VCINSTALLDIR%\Tools\MSVC\%VCToolsVersion%



:VS_VARIABLES_ARE_SET_UP
set WIN_INCLUDE_FLAGS=-I"%VCToolsInstallDir%\include"
set WIN_INCLUDE_FLAGS=%WIN_INCLUDE_FLAGS% -I"%WindowsSdkDir%\Include\%WindowsSDKVersion%\ucrt"
set WIN_INCLUDE_FLAGS=%WIN_INCLUDE_FLAGS% -I"%WindowsSdkDir%\Include\%WindowsSDKVersion%\um"
set WIN_INCLUDE_FLAGS=%WIN_INCLUDE_FLAGS% -I"%WindowsSdkDir%\Include\%WindowsSDKVersion%\shared"
set WIN_LIB_FLAGS=-L"%VCToolsInstallDir%\lib\x64"
set WIN_LIB_FLAGS=%WIN_LIB_FLAGS% -L"%WindowsSdkDir%\Lib\%WindowsSDKVersion%\um\x64"
set WIN_LIB_FLAGS=%WIN_LIB_FLAGS% -L"%WindowsSdkDir%\Lib\%WindowsSDKVersion%\ucrt\x64"

set ClangExe= D:\Compilers\LLVM\bin\clang++.exe
REM remove -march=native when macros are introduced to SIMD!!!
set CompilerFlags= -O3 -march=native -fuse-ld=lld -std=c++23 -g -gcodeview -fms-runtime-lib=static_dbg -Wno-microsoft-string-literal-from-predefined
set CompilerFlags= -DINTERNAL_BUILD=1 -DSLOW_VALIDATION=1 %CompilerFlags%
set LinkerFlags= %WIN_LIB_FLAGS% -l user32 -l gdi32 -l ole32 -l winmm -l advapi32 -l opengl32
set DllExports=-Xlinker /export:GameMainLoopFrame -Xlinker /export:GameFillSoundBuffer -Xlinker /export:DebugInit -Xlinker /export:DebugFinishFrame
pushd ..\build
  echo %cd%
  del *.pdb > NUL 2> NUL
  REM %ClangExe% %CompilerFlags% -S ..\code\engine.cpp -- -S for assembly => for optimization
  %ClangExe% %CompilerFlags% ..\code\engine.cpp -shared -o engine.dll %WIN_LIB_FLAGS% %DllExports% -Xlinker /pdb:engine%random%.pdb 
  %ClangExe% %CompilerFlags% ..\code\win32_main.cpp -o win32_main.exe -Wl,%LinkerFlags%
popd