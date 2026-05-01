cl /nologo /Zi /W4 /std:c11 /fsanitize=address json.c /link /debug
.\json.exe %*
