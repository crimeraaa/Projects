@echo off
set CL_FLAGS=/nologo /Zi /std:c11 /fsanitize=address
set LINK_FLAGS=/link /debug /out:test.exe

pushd %~dp0
cl %CL_FLAGS% test.c %LINK_FLAGS%
popd

%~dp0\test.exe %*

set LINK_FLAGS=""
set CL_FLAGS=""
