@echo OFF
set CC_FLAGS=/nologo /W4 /Zi /fsanitize=address /std:c11
set LINK_FLAGS=/link /debug

cl %CC_FLAGS% primer.c %LINK_FLAGS%

set LINK_FLAGS=""
set CC_FLAGS=""
