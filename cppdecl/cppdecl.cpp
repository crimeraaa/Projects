#include <cstdio>

#include "arena.cpp"
#include "lexer.cpp"
#include "parser.cpp"

static String
read_line(Slice<char> buffer, std::FILE *stream)
{
    Slice<char> line;
    line.data = std::fgets(&buffer[0], cast(int)len(buffer), stream);
    if (line.data != nullptr) {
        line.len  = strcspn(line.data, "\r\n");
        line.data[line.len] = 0;
    }
    return String{raw_data(line), len(line)};
}

static bool
got_line(const char *prompt, Slice<char> buffer, String &line)
{
    std::fputs(prompt, stdout);
    line = read_line(buffer, stdin);
    if (line.data == nullptr) {
        std::fputc('\n', stdout);
        return false;
    }
    return true;
}

int
main()
{
    char arena_buf[64], input_buf[64];
    Arena arena({arena_buf, sizeof(arena_buf)});

    for (;;) {
        String line;
        if (!got_line("cppdecl>", {input_buf, sizeof(input_buf)}, line)) {
            break;
        }
        Parser parser(line, &arena);
        parser.program();
    }
    return 0;
}
