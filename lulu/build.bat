@echo OFF

set CL_FLAGS=/W4 /Zi /std:c11 /fsanitize=address /Fo:obj\ /Fe:bin\
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

set LINK_FLAGS=""
set CL_FLAGS=""
