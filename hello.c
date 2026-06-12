#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *
get_string(char *buffer, size_t buffer_size)
{
    char *input;
    size_t newline_index;

    input = fgets(buffer, buffer_size, stdin);

    // EOF or error?
    if (input == NULL) {
        return NULL;
    }

    // Trim newline.
    newline_index = strcspn(input, "\r\n");
    input[newline_index] = '\0';
    return input;
}

int main(void)
{
    const char *name;
    char buffer[256];

    printf("Enter your name: ");
    name = get_string(buffer, sizeof(buffer));
    if (name != NULL) {
        printf("Hi %s!\n", name);
    } else {
        printf("Uh oh!\n");
    }

    return 0;
}
