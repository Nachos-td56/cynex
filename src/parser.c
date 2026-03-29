/* parser.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "value.h"
#include "variable.h"
#include "lexer.h"

/* ---------- Parser initialization ---------- */
void parser_init(Parser* p, const char* s) {
    p->lx.src = s;
    p->lx.pos = 0;
    lexer_next(&p->lx);
}

/* ---------- Parser helpers ---------- */
int accept(Parser* p, TokenType t) {
    if (p->lx.cur.type == t) {
        lexer_next(&p->lx);
        return 1;
    }
    return 0;
}

int expect(Parser* p, TokenType t, const char* errmsg) {
    if (p->lx.cur.type == t) {
        lexer_next(&p->lx);
        return 1;
    }
    if (errmsg) {
        fprintf(stderr, "Parse error: %s\n", errmsg);
    }
    return 0;
}

/* ---------- Expression parsing ---------- */
Value parse_concat(Parser* p, int* printed) {
    Value v = parse_additive(p, printed);
    while (p->lx.cur.type == T_CONCAT) {
        lexer_next(&p->lx);
        Value rhs = parse_additive(p, printed);
        char* a = (v.type == VAL_STRING ? strdup(v.string ? v.string : "") : value_to_cstring(&v));
        char* b = (rhs.type == VAL_STRING ? strdup(rhs.string ? rhs.string : "") : value_to_cstring(&rhs));
        char* cat = malloc(strlen(a) + strlen(b) + 1);
        strcpy(cat, a);
        strcat(cat, b);
        free(a); free(b);
        free_value(&v);
        free_value(&rhs);
        v = make_string(cat);
        free(cat);
    }
    return v;
}

Value parse_additive(Parser* p, int* printed) {
    Value v = parse_term(p, printed);
    while (p->lx.cur.type == T_PLUS || p->lx.cur.type == T_MINUS) {
        char op = (p->lx.cur.type == T_PLUS) ? '+' : '-';
        lexer_next(&p->lx);
        Value rhs = parse_term(p, printed);
        Value tmp = binary_arith(v, rhs, op);
        free_value(&v);
        free_value(&rhs);
        v = tmp;
    }
    return v;
}

Value parse_term(Parser* p, int* printed) {
    Value v = parse_factor(p, printed);
    while (p->lx.cur.type == T_STAR || p->lx.cur.type == T_SLASH) {
        char op = (p->lx.cur.type == T_STAR) ? '*' : '/';
        lexer_next(&p->lx);
        Value rhs = parse_factor(p, printed);
        Value tmp = binary_arith(v, rhs, op);
        free_value(&v);
        free_value(&rhs);
        v = tmp;
    }
    return v;
}

Value parse_factor(Parser* p, int* printed) {
    if (accept(p, T_PLUS)) return parse_factor(p, printed);
    if (accept(p, T_MINUS)) {
        Value v = parse_factor(p, printed);
        double n;
        if (value_to_number(&v, &n)) {
            free_value(&v);
            return make_number(-n);
        }
        fprintf(stderr, "Unary - on non-number\n");
        free_value(&v);
        return make_number(0);
    }
    return parse_primary(p, printed);
}

Value parse_primary(Parser* p, int* printed) {
    if (p->lx.cur.type == T_NUMBER) {
        double n = p->lx.cur.number;
        lexer_next(&p->lx);
        return make_number(n);
    }
    if (p->lx.cur.type == T_STRING) {
        char tmp[LINE_MAX];
        strncpy(tmp, p->lx.cur.text, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        lexer_next(&p->lx);
        return make_string(tmp);
    }

    if (p->lx.cur.type == T_IDENT) {
        char name[MAX_NAME];
        strncpy(name, p->lx.cur.text, MAX_NAME - 1);
        name[MAX_NAME - 1] = '\0';
        lexer_next(&p->lx);

        /* Special constants - check FIRST */
        if (strcmp(name, "true") == 0)  return make_number(1);
        if (strcmp(name, "false") == 0) return make_number(0);

        /* Built-in print(...) */
        if (strcmp(name, "print") == 0 && accept(p, T_LPAREN)) {
            int first = 1;
            while (p->lx.cur.type != T_RPAREN && p->lx.cur.type != T_EOF) {
                Value arg = parse_concat(p, printed);
                if (!first) printf(" ");
                first = 0;

                char* s = value_to_cstring(&arg);
                printf("%s", s ? s : "");
                free(s);

                free_value(&arg);
                accept(p, T_COMMA);
            }
            expect(p, T_RPAREN, "expected ')' after print arguments");
            printf("\n");
            if (printed) *printed = 1;
            return make_string("");   // silent return (less visible on error)
        }

        /* Normal variable */
        VarEntry* v = find_var(name);
        if (!v) {
            fprintf(stderr, "Undefined variable: %s\n", name);
            return make_string("");   // no extra 0 printed
        }
        if (v->value.type == VAL_NUMBER) {
            return make_number(v->value.number);
        }
        else {
            return make_string(v->value.string ? v->value.string : "");
        }
    }

    if (accept(p, T_LPAREN)) {
        Value v = parse_concat(p, printed);
        expect(p, T_RPAREN, "expected ')'");
        return v;
    }

    fprintf(stderr, "Unexpected token: %s\n", p->lx.cur.text);
    lexer_next(&p->lx);   // eat bad token to avoid infinite loop
    return make_string("");
}

/* ---------- Statement parsing (REPL entry point) ---------- */
void parse_statement(Parser* p) {
    /* if ... then ... end */
    if (p->lx.cur.type == T_IDENT && strcmp(p->lx.cur.text, "if") == 0) {
        lexer_next(&p->lx);
        Value cond = parse_concat(p, NULL);
        double cond_val = 0;
        value_to_number(&cond, &cond_val);
        free_value(&cond);

        if (p->lx.cur.type != T_IDENT || strcmp(p->lx.cur.text, "then") != 0) {
            fprintf(stderr, "Parse error: expected 'then'\n");
            return;
        }
        lexer_next(&p->lx);

        int found_end = 0;
        while (p->lx.cur.type != T_EOF) {
            if (p->lx.cur.type == T_IDENT && strcmp(p->lx.cur.text, "end") == 0) {
                lexer_next(&p->lx);
                found_end = 1;
                break;
            }
            if (cond_val != 0) {
                parse_statement(p);
            }
            else {
                while (p->lx.cur.type != T_EOF &&
                    !(p->lx.cur.type == T_IDENT &&
                        (strcmp(p->lx.cur.text, "end") == 0 ||
                            strcmp(p->lx.cur.text, "if") == 0 ||
                            strcmp(p->lx.cur.text, "local") == 0 ||
                            strcmp(p->lx.cur.text, "string") == 0 ||
                            strcmp(p->lx.cur.text, "int") == 0 ||
                            strcmp(p->lx.cur.text, "print") == 0))) {
                    lexer_next(&p->lx);
                }
            }
        }
        if (!found_end) fprintf(stderr, "Parse error: expected 'end' for if\n");
        return;
    }

    /* Assignment: name = expr */
    if (p->lx.cur.type == T_IDENT) {
        char name[MAX_NAME];
        strncpy(name, p->lx.cur.text, MAX_NAME - 1);
        name[MAX_NAME - 1] = '\0';

        Token saved = p->lx.cur;  // save token state
        lexer_next(&p->lx);

        if (p->lx.cur.type == T_ASSIGN) {
            lexer_next(&p->lx);  // consume =
            Value val = parse_concat(p, NULL);

            VarEntry* v = find_var(name);
            if (v) {
                free_value(&v->value);
                v->value = val;
            }
            else {
                // Auto-create on first assignment (like Lua globals)
                v = create_var(name);
                if (v) {
                    free_value(&v->value);
                    v->value = val;
                }
                else {
                    free_value(&val);
                }
            }
            // Assignment statements usually print nothing
            return;
        }

        // Not an assignment -> restore token so expression / other rules can see it
        p->lx.cur = saved;
    }

    /* local / string / int declaration */
    if (p->lx.cur.type == T_IDENT &&
        (strcmp(p->lx.cur.text, "local") == 0 ||
            strcmp(p->lx.cur.text, "string") == 0 ||
            strcmp(p->lx.cur.text, "int") == 0)) {
        lexer_next(&p->lx);
        if (p->lx.cur.type != T_IDENT) {
            fprintf(stderr, "Expected variable name\n");
            return;
        }
        char name[MAX_NAME];
        strncpy(name, p->lx.cur.text, MAX_NAME - 1);
        name[MAX_NAME - 1] = '\0';
        lexer_next(&p->lx);

        Value val = make_number(0);
        if (accept(p, T_ASSIGN)) val = parse_concat(p, NULL);

        VarEntry* v = create_var(name);
        if (v) {
            free_value(&v->value);
            v->value = val;
        }
        else free_value(&val);
        return;
    }

    /* Bare expression - auto-print result if not already printed by print() */
    int printed = 0;
    Value result = parse_concat(p, &printed);
    if (!printed) {
        char* s = value_to_cstring(&result);
        if (s && *s != '\0') {
            printf("%s\n", s);
        }
        free(s);
    }
    free_value(&result);

    /* Eat any leftover tokens to avoid getting stuck */
    while (p->lx.cur.type != T_EOF && p->lx.cur.type != '\n' && p->lx.cur.type != T_UNKNOWN) {
        lexer_next(&p->lx);
    }
}
