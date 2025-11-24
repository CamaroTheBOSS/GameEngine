@echo off
IF NOT EXIST ..\build (
	mkdir ..\build
)
set CompilerFlags= -std=C++23 -g -fms-runtime-lib=static_dbg -O0 -DHANDMADE_INTERNAL_BUILD -DHANDMADE_SLOW
set LinkerFlags= -l user32 gdi32 ole32 winmm
pushd ..\build
  echo %cd%
  del *.pdb > NUL 2> NUL
  clang %CompilerFlags% ..\code\engine.cpp -LD  -incremental:no -opt:ref -PDB:engine%random%.pdb -EXPORT:GameMainLoopFrame
  cl %CompilerFlags% ..\code\win32_main.cpp /link %LinkerFlags%
popd