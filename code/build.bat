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
set WIN_INCLUDE_FLAGS=/I "%VS_TOOLKIT_DIR%\include"
set WIN_INCLUDE_FLAGS=%WIN_INCLUDE_FLAGS% /I "%VS_SDK_DIR%\Include\%VS_SDK_VERSION%\ucrt"
set WIN_INCLUDE_FLAGS=%WIN_INCLUDE_FLAGS% /I "%VS_SDK_DIR%\Include\%VS_SDK_VERSION%\um"
set WIN_INCLUDE_FLAGS=%WIN_INCLUDE_FLAGS% /I "%VS_SDK_DIR%\Include\%VS_SDK_VERSION%\shared"
set WIN_LIB_FLAGS=/LIBPATH:"%VS_TOOLKIT_DIR%\lib\x64"
set WIN_LIB_FLAGS=%WIN_LIB_FLAGS% /LIBPATH:"%VS_SDK_DIR%\Lib\%VS_SDK_VERSION%\um\x64"
set WIN_LIB_FLAGS=%WIN_LIB_FLAGS% /LIBPATH:"%VS_SDK_DIR%\Lib\%VS_SDK_VERSION%\ucrt\x64"
set PATH=%MSVC_TOOLS_PATH%;%PATH%

set CompilerFlags= -Od -nologo -GR- -MTd -Oi -W4 -WX -wd4100 -wd4189 -wd4505 -wd4005 -Zi -Fm -std:c++20 %WIN_INCLUDE_FLAGS%
set CompilerFlags= -DINTERNAL_BUILD=1 -DSLOW_VALIDATION=1 %CompilerFlags%
set LinkerFlags= %WIN_LIB_FLAGS% -incremental:no -opt:ref user32.lib gdi32.lib ole32.lib winmm.lib
pushd ..\build
  echo %cd%
  del *.pdb > NUL 2> NUL

  REM Code generator
  cl %CompilerFlags% ..\code\tools_code_generator.cpp /link -incremental:no %WIN_LIB_FLAGS%
  pushd ..\code 
  ..\build\tools_code_generator.exe > engine_meta.cpp
  popd
  
  REM Asset Composer
  REM cl %CompilerFlags% ..\code\tools_asset_file_composer.cpp /link -incremental:no gdi32.lib user32.lib %WIN_LIB_FLAGS% 

  REM Platform layer + game code
  cl %CompilerFlags% -O2 ..\code\engine_optimized.cpp /c -Foengine_optimized.obj
  cl %CompilerFlags% ..\code\engine.cpp -LD engine_optimized.obj /link %WIN_LIB_FLAGS% -incremental:no -opt:ref -PDB:engine%random%.pdb -EXPORT:GameMainLoopFrame -EXPORT:GameFillSoundBuffer -EXPORT:DebugInit -EXPORT:DebugFinishFrame
  cl %CompilerFlags% ..\code\win32_main.cpp /link %LinkerFlags%
popd
endlocal