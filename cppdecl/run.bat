@set SRC="cppdecl"
@set EXT="cpp"

cl /nologo /W3 /EHsc /Zi /fsanitize=address /std:c++17 %SRC%.%EXT% /link /debug
%~dp0\%SRC%.exe

@set SRC=""
@set EXT=""
