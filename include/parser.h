#ifndef CYNEX_PARSER_H
#define CYNEX_PARSER_H

#include "lexer.h"
#include "value.h"

typedef struct {
    Lexer lx;
} Parser;

void parser_init(Parser* p, const char* s);

// Forward declarations of expression levels
Value parse_concat(Parser* p, int* printed);
Value parse_additive(Parser* p, int* printed);
Value parse_term(Parser* p, int* printed);
Value parse_factor(Parser* p, int* printed);
Value parse_primary(Parser* p, int* printed);

// Statement level
void parse_statement(Parser* p);

// Helpers
int accept(Parser* p, TokenType t);
int expect(Parser* p, TokenType t, const char* errmsg);

#endif /* CYNEX_PARSER_H */