@echo OFF

set CL_FLAGS=/nologo /W3 /Zi /std:c11 /Fo:obj\ /Fe:bin\
set LINK_FLAGS=/link /debug

pushd %~dp0

if not exist obj\ (
    mkdir obj
)

if not exist bin\ (
    mkdir bin
)

cl.exe %CL_FLAGS% lulu.c %LINK_FLAGS%

popd

echo Command line: %~dp0\bin\lulu.exe %*
%~dp0\bin\lulu.exe %*

set LINK_FLAGS=""
set CL_FLAGS=""
