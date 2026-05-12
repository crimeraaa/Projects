@set OUT=vm.exe
@set SRC=vm.c
@set CC_FLAGS=/nologo /Zi /Os /std:c11 /fsanitize=address
@set LD_FLAGS=/debug

cl %CC_FLAGS% %SRC% /link %LD_FLAGS%

@set LD_FLAGS=""
@set CC_FLAGS=""
@set SRC=""

%~dp0.\%OUT%

@set OUT=""
