@echo off
IF NOT EXIST ..\build (
	mkdir ..\build
)
set ClangExe= D:\Compilers\LLVM\bin\clang++.exe
REM remove -march=native when macros are introduced to SIMD!!!
set CompilerFlags= -march=native -fuse-ld=lld -std=c++23 -g -gcodeview -fms-runtime-lib=static_dbg -O0 -DHANDMADE_INTERNAL_BUILD -DHANDMADE_SLOW
set LinkerFlags= -l user32 -l gdi32 -l ole32 -l winmm
pushd ..\build
  echo %cd%
  del *.pdb > NUL 2> NUL
  %ClangExe% %CompilerFlags% -S ..\code\engine.cpp
  %ClangExe% %CompilerFlags% ..\code\engine.cpp -shared -o engine.dll -Xlinker /pdb:engine%random%.pdb -Xlinker /export:GameMainLoopFrame 
  %ClangExe% %CompilerFlags% ..\code\win32_main.cpp -o win32_main.exe -Wl,%LinkerFlags%
popd