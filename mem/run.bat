@echo off
set CL_FLAGS=/nologo /Zi /std:c11 /fsanitize=address
set LINK_FLAGS=/link /debug

pushd %~dp0
cl %CL_FLAGS% mem.c %LINK_FLAGS%
popd

%~dp0\mem.exe %*

set LINK_FLAGS=""
set CL_FLAGS=""
