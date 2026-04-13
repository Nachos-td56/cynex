// repl.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "repl.h"
#include "parser.h"
#include "variable.h"

#define LINE_MAX 2048
#define BUFFER_MAX 32768   // max size for multiline buffer

static int execute_buffer(const char* buffer) {
    Parser p;
    parser_init(&p, buffer);

    if (p.lx.cur.type == T_EOF) {
        return 0;
    }

    if (p.lx.cur.type == T_IDENT && strcmp(p.lx.cur.text, "exit") == 0) {
        return 1;
    }

    parse_statement(&p);
    return 0;
}

void repl_run(void) {
    char line[LINE_MAX];
    char buffer[BUFFER_MAX] = {0};
    int in_block = 0;           // are we inside if/while/etc?
    int should_exit = 0;

    printf("Cynex v0.16.2 REPL (multiline enabled)\n");
    printf("Type 'exit' to quit.\n\n");

    while (!should_exit) {
        printf(in_block ? ">> " : "> ");

        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }

        // remove trailing newline
        line[strcspn(line, "\n")] = '\0';

        // skip empty lines when not in block
        if (!in_block) {
            const char* p = line;
            while (*p && isspace((unsigned char)*p)) p++;
            if (*p == '\0') continue;
        }

        // Append to buffer
        if (strlen(buffer) + strlen(line) + 2 >= BUFFER_MAX) {
            fprintf(stderr, "Input buffer too large!\n");
            buffer[0] = '\0';
            in_block = 0;
            continue;
        }

        if (buffer[0] != '\0') strcat(buffer, "\n");
        strcat(buffer, line);

        // Try to execute
        should_exit = execute_buffer(buffer);

        // Simple heuristic: if line contains "end" at start (after whitespace), end block
        // This is good enough for early version
        {
            const char* trimmed = line;
            while (*trimmed && isspace((unsigned char)*trimmed)) trimmed++;

            if (strncmp(trimmed, "end", 3) == 0) {
                in_block = 0;
                buffer[0] = '\0';           // clear buffer after complete statement
            }
            else if (strncmp(trimmed, "if", 2) == 0 ||
                     strncmp(trimmed, "while", 5) == 0) {
                in_block = 1;
            }
            else if (!in_block) {
                // Single-line statement finished
                buffer[0] = '\0';
            }
        }
    }

    printf("\nGoodbye!\n");
}
