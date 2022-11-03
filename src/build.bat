@echo off

set "compilerflags=/I..\src\ /Zi /W4 /WX /wd4100 /wd4189 /MT /nologo /GR- /EHa-"
set "linkerflags=/DEBUG:FULL /INCREMENTAL:NO /opt:ref User32.lib Gdi32.lib Opengl32.lib glew32.lib"
mkdir ..\build
pushd ..\build
cl ..\src\main.cpp %compilerflags% /link %linkerflags%
popd