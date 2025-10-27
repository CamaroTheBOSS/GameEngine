@echo off
IF NOT EXIST ..\build (
	mkdir ..\build
)
pushd ..\build
  echo %cd%
  cl /nologo /GR- /MT /Od /Oi /W4 /WX /wd4100 /wd4189 /DHANDMADE_INTERNAL_BUILD /Z7 /Fm /std:c++20 ..\code\win32_main.cpp user32.lib gdi32.lib ole32.lib winmm.lib
popd