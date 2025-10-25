@echo off
mkdir ..\build
pushd ..\build
  echo %cd%
  cl /Zi /std:c++20 ..\code\win32_main.cpp user32.lib gdi32.lib ole32.lib
popd