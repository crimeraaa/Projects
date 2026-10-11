@echo OFF

set CC=cl.exe
set ASAN_FLAGS=/fsanitize=address
set LINK_FLAGS=/link /debug
set CC_FLAGS=/nologo /std:c11 /W3 /Zi %ASAN_FLAGS% /Fo:obj\ /Fe:bin\

rem Get the *drive* and *path* of the current script (%0).
pushd %~dp0

if not exist obj\ mkdir obj
if not exist bin\ mkdir bin

%CC% %CC_FLAGS% hash.c %LINK_FLAGS%

popd

rem Don't execute if an error occurred during compilation.
if %ERRORLEVEL% NEQ 0 exit /B 1

@echo ON
%~dp0.\bin\hash.exe %*
@echo OFF

set CC=""
set CC_FLAGS=""
set LINK_FLAGS=""
set ASAN_FLAGS=""
