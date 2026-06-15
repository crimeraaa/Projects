@echo OFF
pushd %~dp0

set SRC=cppdecl
set EXT=cpp
set OUT=bin\%SRC%.exe
set CL_FLAGS=/nologo /W3 /EHsc /Zi /fsanitize=address /std:c++17 /Fo:obj\ /Fe:bin\ /Fd:bin\
set LINK_FLAGS=/link /debug
if not exist obj (
    mkdir obj
)

if not exist bin (
    mkdir bin
)

cl.exe %CL_FLAGS% "%SRC%.%EXT%" %LINK_FLAGS%
%OUT% %*

set LINK_FLAGS=""
set CL_FLAGS=""
set OUT=""
set SRC=""
set EXT=""
popd
