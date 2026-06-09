@echo OFF
set CL_FLAGS=/nologo /Zi /W4 /std:c11 /fsanitize=address
set LINK_FLAGS=/link /debug

cl %CL_FLAGS% ls.c %LINK_FLAGS%

set CL_FLAGS=""
set LINK_FLAGS=""
