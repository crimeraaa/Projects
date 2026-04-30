cl /nologo /Zi /W4 /std:c11 /fsanitize=address test.c /link /debug
.\test.exe %*
