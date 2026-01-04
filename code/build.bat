@echo off
IF NOT EXIST ..\build (
	mkdir ..\build
)
set CompilerFlags= -nologo -GR- -MTd -O2 -Oi -W4 -WX -wd4100 -wd4189 -wd4505 -wd4005 -DHANDMADE_INTERNAL_BUILD -DHANDMADE_SLOW -Z7 -Fm -std:c++20
set LinkerFlags= -incremental:no -opt:ref user32.lib gdi32.lib ole32.lib winmm.lib
pushd ..\build
  echo %cd%
  del *.pdb > NUL 2> NUL
  cl %CompilerFlags% ..\code\engine.cpp -LD /link -incremental:no -opt:ref -PDB:engine%random%.pdb -EXPORT:GameMainLoopFrame
  cl %CompilerFlags% ..\code\win32_main.cpp /link %LinkerFlags%
popd