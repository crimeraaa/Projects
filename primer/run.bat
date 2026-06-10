@echo OFF

rem Expands to *d*rive letter and *p*ath of argument 0 (this batch file).
pushd %~dp0
call build.bat && primer.exe %*
popd

echo Program exited with error code %ERRORLEVEL%.
