@set SRC="cppdecl"
@set EXT="cpp"

cl /nologo /Zi /std:c++17 /fsanitize=address %SRC%.%EXT% /link /debug
%~dp0\%SRC%.exe

@set SRC=""
@set EXT=""
