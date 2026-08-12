@echo OFF

set SRC=main.c
set OBJ=obj\
set BIN=bin\tui.exe
set CC=cl.exe
set CC_FLAGS=/nologo /std:c11 /W3 /Zi /fsanitize=address /Fo:%OBJ% /Fe:%BIN%

pushd %~dp0

if not exist obj (
    mkdir obj
)

if not exist bin (
    mkdir bin
)

%CC% %CC_FLAGS% %SRC%

popd %~dp0

@rem %~dp0%BIN% %*

set "BIN="
set "OBJ="
set "SRC="
set "CC_FLAGS="
set "CC="
