@echo off
IF NOT EXIST ..\build (
	mkdir ..\build
)
set CompilerFlags= -Od -nologo -GR- -MTd -Oi -W4 -WX -wd4100 -wd4189 -wd4505 -wd4005 -DHANDMADE_INTERNAL_BUILD -DHANDMADE_SLOW -Zi -Fm -std:c++20
set LinkerFlags= -incremental:no -opt:ref user32.lib gdi32.lib ole32.lib winmm.lib
pushd ..\build
  echo %cd%
  del *.pdb > NUL 2> NUL
  REM Engine Asset Composer
  cl %CompilerFlags% ..\code\engine_asset_file_composer.cpp /link -incremental:no

  REM Platform layer + game code
  cl %CompilerFlags% ..\code\engine.cpp -LD /link -incremental:no -opt:ref -PDB:engine%random%.pdb -EXPORT:GameMainLoopFrame
  cl %CompilerFlags% ..\code\win32_main.cpp /link %LinkerFlags%
popd