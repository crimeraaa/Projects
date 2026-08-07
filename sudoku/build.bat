@echo OFF
rem The following flags assuming the CWD is the project's.
set CC=cl.exe
set CC_FLAGS=/nologo /Zi /std:c11 /fsanitize=address /Fo:obj\ /Fe:bin\
set SRC=sudoku.c
set OUT=%~dp0\bin\sudoku.exe

pushd %~dp0
if not exist obj (
    mkdir obj
)

if not exist bin (
    mkdir bin
)

%CC% %CC_FLAGS% %SRC%
popd

%OUT% %*

set "CC="
set "CC_FLAGS="
set "SRC="
set "OUT="
