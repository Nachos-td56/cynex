// repl.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "repl.h"
#include "parser.h"
#include "variable.h"

#define LINE_MAX 2048

static int execute_line(const char* line) {
    Parser p;
    parser_init(&p, line);

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

    printf("Cynex v0.16 REPL\nType 'exit' to quit.\n");

    while (1) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }

        // remove trailing newline
        line[strcspn(line, "\n")] = '\0';

        // skip leading whitespace
        const char* p = line;
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
        if (*p == '\0') {
            continue;   // empty line
        }

        int should_exit = execute_line(line);
        if (should_exit) {
            break;
        }
    }
}