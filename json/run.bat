@echo OFF

set CL_FLAGS=/nologo /Zi /W4 /std:c11 /fsanitize=address
set LINK_FLAGS=/link /debug

pushd %~dp0
cl %CL_FLAGS% json.c %LINK_FLAGS%
popd %~dp0

%~dp0\json.exe %*

set CL_FLAGS=""
set LINK_FLAGS=""
