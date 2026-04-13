// repl.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "repl.h"
#include "parser.h"
#include "variable.h"

#define LINE_MAX    2048
#define BUFFER_MAX  65536   // 64KB should be plenty

static char buffer[BUFFER_MAX] = { 0 };
static int  in_block = 0;

static int is_block_starter(const char* line) {
    const char* p = line;
    while (*p && isspace((unsigned char)*p)) p++;
    return strncmp(p, "if", 2) == 0 || strncmp(p, "while", 5) == 0;
}

static int is_block_ender(const char* line) {
    const char* p = line;
    while (*p && isspace((unsigned char)*p)) p++;
    return strncmp(p, "end", 3) == 0;
}

static int execute_buffer(void) {
    if (buffer[0] == '\0') return 0;

    Parser p;
    parser_init(&p, buffer);

    if (p.lx.cur.type == T_IDENT && strcmp(p.lx.cur.text, "exit") == 0) {
        return 1;
    }

    parse_statement(&p);

    // If we got here without crashing, consider it done
    return 0;
}

void repl_run(void) {
    char line[LINE_MAX];

    printf("Cynex v0.16.2 REPL - With long awaited Multiline support\n");
    printf("Type 'exit' to quit.\n\n");

    while (1) {
        printf(in_block ? ">> " : "> ");

        if (!fgets(line, sizeof(line), stdin)) break;

        line[strcspn(line, "\n")] = '\0';

        // Skip completely empty lines when not in block
        if (!in_block) {
            const char* p = line;
            while (*p && isspace((unsigned char)*p)) p++;
            if (*p == '\0') continue;
        }

        // Append to buffer
        if (strlen(buffer) + strlen(line) + 3 >= BUFFER_MAX) {
            fprintf(stderr, "Input too large!\n");
            buffer[0] = '\0';
            in_block = 0;
            continue;
        }

        if (buffer[0]) strcat(buffer, "\n");
        strcat(buffer, line);

        // Detect block start
        if (!in_block && is_block_starter(line)) {
            in_block = 1;
            continue;   // wait for more lines
        }

        // If we hit an 'end' line, execute the whole thing
        if (is_block_ender(line)) {
            int should_exit = execute_buffer();
            buffer[0] = '\0';
            in_block = 0;
            if (should_exit) break;
            continue;
        }

        // For normal single-line statements (no block)
        if (!in_block) {
            int should_exit = execute_buffer();
            buffer[0] = '\0';
            if (should_exit) break;
        }
        // else: still inside block then keep accumulating
    }
}
