@echo OFF

set ASAN_FLAGS=/fsanitize=address
rem set ASAN_FLAGS=
set LINK_FLAGS=/link /debug
set CL_FLAGS=/nologo /W3 /Zi %ASAN_FLAGS% /std:c11 /Fo:obj\ /Fe:bin\

rem Get the *drive* and *path* of the current script (%0).
pushd %~dp0

if not exist obj\ mkdir obj
if not exist bin\ mkdir bin

cl.exe %CL_FLAGS% lulu.c %LINK_FLAGS%

popd

rem Don't execute if an error occurred during compilation.
if %ERRORLEVEL% NEQ 0 exit /B 1

echo Command line: %~dp0.\bin\lulu.exe %*
%~dp0.\bin\lulu.exe %*

set CL_FLAGS=""
set LINK_FLAGS=""
set ASAN_FLAGS=""
