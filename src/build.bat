@echo off

set "compilerflags=/I..\src\ /Zi /W4 /WX /wd4100 /wd4127 /wd4189 /wd4201 /MT /nologo /GR- /EHa-"
set "linkerflags=/DEBUG:FULL /INCREMENTAL:NO /opt:ref User32.lib Gdi32.lib Opengl32.lib glew32.lib ..\..\vcpkg\installed\x64-windows\lib\assimp-vc143-mt.lib"
mkdir ..\build
pushd ..\build
cl ..\src\main.cpp ..\src\imgui\*.cpp %compilerflags% /link %linkerflags%
popd