// parser.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"
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
        if (cat == NULL) {
            fprintf(stderr, "Error: Out of memory. Cynex cannot continue and must close.\n");
            free(a); free(b);
            free_value(&v);
            free_value(&rhs);
            cynex_sleep(1000);
            exit(1);               // consistent with xstrdup / other allocs
        }

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

        /* Special constants — check FIRST */
        if (strcmp(name, "true") == 0)  return make_number(1);
        if (strcmp(name, "false") == 0) return make_number(0);

        /* Built-in print(...) */
        if (strcmp(name, "print") == 0) {
            if (accept(p, T_LPAREN)) {
                int first = 1;
                char output[LINE_MAX * 2] = { 0 };   // simple fixed buffer (plenty for now)

                while (p->lx.cur.type != T_RPAREN && p->lx.cur.type != T_EOF) {
                    Value arg = parse_concat(p, printed);
                    if (!first) strcat(output, " ");
                    first = 0;

                    char* s = value_to_cstring(&arg);
                    if (s) {
                        strcat(output, s);
                        free(s);
                    }
                    free_value(&arg);
                    accept(p, T_COMMA);
                }

                if (expect(p, T_RPAREN, "expected ')' after print arguments")) {
                    printf("%s\n", output);        // only print if ) was present
                    if (printed) *printed = 1;
                }
                // else: syntax error already shown, NO output printed
                return make_string("");
            }
            else {
                // bare "print" with no ( )
                fprintf(stderr, "Parse error: expected '(' after print\n");
                return make_string("");
            }
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
    // Skip empty statements (just newlines)
    while (p->lx.cur.type == T_EOF && p->lx.pos == 0) {  // rough
        return;
    }

    /* if ... then ... [else ...] end */
    if (p->lx.cur.type == T_IDENT && strcmp(p->lx.cur.text, "if") == 0) {
        lexer_next(&p->lx);
        Value cond = parse_concat(p, NULL);
        int cond_truthy = is_truthy(&cond);
        free_value(&cond);

        if (p->lx.cur.type != T_IDENT || strcmp(p->lx.cur.text, "then") != 0) {
            fprintf(stderr, "Parse error: expected 'then'\n");
            return;
        }
        lexer_next(&p->lx);

        int found_end = 0;
        int in_else = 0;

        while (p->lx.cur.type != T_EOF) {
            if (p->lx.cur.type == T_IDENT) {
                if (strcmp(p->lx.cur.text, "end") == 0) {
                    lexer_next(&p->lx);
                    found_end = 1;
                    break;
                }
                if (strcmp(p->lx.cur.text, "else") == 0) {
                    lexer_next(&p->lx);
                    in_else = 1;
                    continue;
                }
            }

            // Execute the correct branch
            if (in_else) {
                if (!cond_truthy) {
                    parse_statement(p);     // run else branch
                }
                else {
                    lexer_next(&p->lx);     // skip else branch
                }
            }
            else {
                if (cond_truthy) {
                    parse_statement(p);     // run then branch
                }
                else {
                    lexer_next(&p->lx);     // skip then branch
                }
            }
        }

        if (!found_end) {
            fprintf(stderr, "Parse error: expected 'end' for if\n");
        }
        return;
    }

    /* local / string / int / float / bool declaration  (checked FIRST) */
    if (p->lx.cur.type == T_IDENT &&
        (strcmp(p->lx.cur.text, "local") == 0 ||
            strcmp(p->lx.cur.text, "string") == 0 ||
            strcmp(p->lx.cur.text, "int") == 0 ||
            strcmp(p->lx.cur.text, "float") == 0 ||
            strcmp(p->lx.cur.text, "bool") == 0)) {

        char decl_kw[MAX_NAME];
        strncpy(decl_kw, p->lx.cur.text, MAX_NAME - 1);
        decl_kw[MAX_NAME - 1] = '\0';
        lexer_next(&p->lx);   // consume the keyword (string/int/etc.)

        if (p->lx.cur.type != T_IDENT) {
            fprintf(stderr, "Expected variable name\n");
            return;
        }
        char name[MAX_NAME];
        strncpy(name, p->lx.cur.text, MAX_NAME - 1);
        name[MAX_NAME - 1] = '\0';
        lexer_next(&p->lx);

        Value val = make_number(0);
        if (accept(p, T_ASSIGN)) {
            val = parse_concat(p, NULL);
        }
        else if (strcmp(decl_kw, "string") == 0) {
            val = make_string("");   // string defaults to empty string
        }
        // int / float / bool / local default to 0 (number)

        VarEntry* v = create_var(name);
        if (v) {
            free_value(&v->value);
            v->value = val;
        }
        else free_value(&val);
        return;
    }

    // Assignment: name = expr   (with proper peek that restores lexer position)
    if (p->lx.cur.type == T_IDENT) {
        char name[MAX_NAME];
        strncpy(name, p->lx.cur.text, MAX_NAME - 1);
        name[MAX_NAME - 1] = '\0';

        size_t saved_pos = p->lx.pos;   // backup lexer position!
        Token saved = p->lx.cur;        // backup token

        lexer_next(&p->lx);

        if (p->lx.cur.type == T_ASSIGN) {
            lexer_next(&p->lx);  // consume =

            /* Protect true/false constants */
            if (strcmp(name, "true") == 0 || strcmp(name, "false") == 0) {
                fprintf(stderr, "Parse error: cannot assign to constant '%s'\n", name);
                Value dummy = parse_concat(p, NULL);
                free_value(&dummy);
                return;
            }

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

        // Not assignment -> fully restore lexer state (critical fix!)
        p->lx.pos = saved_pos;
        p->lx.cur = saved;
    }

    // Bare expression - auto-print result if not already printed by print()
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
}
