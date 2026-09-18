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
set MSVC_TOOLS_PATH=%VCToolsInstallDir%\bin\Hostx64\x64
set WIN_INCLUDE_FLAGS=/I "%VCToolsInstallDir%\include"
set WIN_INCLUDE_FLAGS=%WIN_INCLUDE_FLAGS% /I "%WindowsSdkDir%\Include\%WindowsSDKVersion%\ucrt"
set WIN_INCLUDE_FLAGS=%WIN_INCLUDE_FLAGS% /I "%WindowsSdkDir%\Include\%WindowsSDKVersion%\um"
set WIN_INCLUDE_FLAGS=%WIN_INCLUDE_FLAGS% /I "%WindowsSdkDir%\Include\%WindowsSDKVersion%\shared"
set WIN_LIB_FLAGS=/LIBPATH:"%VCToolsInstallDir%\lib\x64"
set WIN_LIB_FLAGS=%WIN_LIB_FLAGS% /LIBPATH:"%WindowsSdkDir%\Lib\%WindowsSDKVersion%\um\x64"
set WIN_LIB_FLAGS=%WIN_LIB_FLAGS% /LIBPATH:"%WindowsSdkDir%\Lib\%WindowsSDKVersion%\ucrt\x64"
set PATH=%MSVC_TOOLS_PATH%;%PATH%

set CompilerFlags= /Zc:nrvo- -O2 -nologo -GR- -MTd -Oi -W4 -WX -wd4100 -wd4189 -wd4505 -wd4005 -Zi -Fm -std:c++20 %WIN_INCLUDE_FLAGS%
set CompilerFlags= -DINTERNAL_BUILD=1 -DSLOW_VALIDATION=1 %CompilerFlags%
set LinkerFlags= %WIN_LIB_FLAGS% -incremental:no -opt:ref user32.lib gdi32.lib ole32.lib winmm.lib advapi32.lib opengl32.lib 
pushd ..\build
  echo %cd%
  del *.pdb > NUL 2> NUL

  REM Code generator
  cl %CompilerFlags% ..\code\tools_code_generator.cpp /link -incremental:no %WIN_LIB_FLAGS%
  pushd ..\code 
  ..\build\tools_code_generator.exe > engine_meta.cpp
  popd
  
  REM Asset Composer
  cl %CompilerFlags% ..\code\tools_asset_file_composer.cpp /link -incremental:no gdi32.lib user32.lib %WIN_LIB_FLAGS% 

  REM Platform layer + game code
  cl %CompilerFlags% ..\code\engine.cpp -LD /link %WIN_LIB_FLAGS% -incremental:no -opt:ref -PDB:engine%random%.pdb -EXPORT:GameMainLoopFrame -EXPORT:GameFillSoundBuffer -EXPORT:DebugInit -EXPORT:DebugFinishFrame
  cl %CompilerFlags% ..\code\win32_main.cpp /link %LinkerFlags%
popd
endlocal