@echo OFF
rem The following flags assuming the CWD is the project's.
set CC=cl.exe
set SRC=main.c
set OBJ=obj\sudoku.obj
set OUT=bin\sudoku.exe
set CC_FLAGS=/nologo /Zi /std:c11 /fsanitize=address /Fo:%OBJ% /Fe:%OUT%

pushd %~dp0
if not exist obj (
    mkdir obj
)

if not exist bin (
    mkdir bin
)

%CC% %CC_FLAGS% %SRC%
popd

%~dp0%OUT% %*

set "CC="
set "CC_FLAGS="
set "SRC="
set "OBJ="
set "OUT="
