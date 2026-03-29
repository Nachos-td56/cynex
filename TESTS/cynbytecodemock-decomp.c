#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t type;
    uint8_t length;
    char* data;
} Constant;

// Duplicate string helper
static char* dupstr(const char* s) {
    size_t n = strlen(s) + 1;
    char* r = (char*)malloc(n);
    if (r) memcpy(r, s, n);
    return r;
}

int main() {
    FILE* f = fopen("concat_example.cynb", "rb");
    if (!f) { perror("open"); return 1; }

    // ---- Header ----
    char magic[4];
    uint8_t version, flags, const_count;
    uint32_t code_size;

    fread(magic, 1, 4, f);
    fread(&version, 1, 1, f);
    fread(&flags, 1, 1, f);
    fread(&code_size, 4, 1, f);
    fread(&const_count, 1, 1, f);

    // ---- Constants ----
    Constant constants[256];
    for (uint8_t i = 0; i < const_count; i++) {
        uint8_t type, len;
        fread(&type, 1, 1, f);
        fread(&len, 1, 1, f);

        char* data = (char*)malloc(len + 1);
        fread(data, 1, len, f);
        data[len] = '\0';

        constants[i] = (Constant){ type, len, data };
    }

    // ---- Bytecode ----
    uint8_t* bytecode = (uint8_t*)malloc(code_size);
    fread(bytecode, 1, code_size, f);
    fclose(f);

    // ---- Decompiler VM ----
    char* stack[256];
    int sp = 0;
    char* variables[256] = { 0 };

    FILE* out = fopen("decompiled_source.txt", "w");
    if (!out) { perror("out"); return 1; }

    size_t pc = 0;
    while (pc < code_size) {
        uint8_t op = bytecode[pc++];

        // LOAD_CONST: push as expression representing the constant
        if (op == 0x01) {
            uint8_t cidx = bytecode[pc++];
            char buf[512];
            sprintf_s(buf, sizeof(buf), "\"%s\"", constants[cidx].data);
            stack[sp++] = dupstr(buf);
        }
        // STORE_VAR: pop top of stack into variable
        else if (op == 0x02) {
            uint8_t vidx = bytecode[pc++];
            variables[vidx] = stack[--sp];
            fprintf(out, "var%d = %s;\n", vidx, variables[vidx]);
        }
        // LOAD_VAR: push variable expression
        else if (op == 0x03) {
            uint8_t vidx = bytecode[pc++];
            stack[sp++] = dupstr(variables[vidx]);
        }
        // CONCAT: pop two expressions, combine into new expression
        else if (op == 0x05) {
            char* b = stack[--sp];
            char* a = stack[--sp];
            char buf[1024];
            sprintf_s(buf, sizeof(buf), "%s .. %s", a, b);
            stack[sp++] = dupstr(buf);
        }
        // PRINT: pop expression and emit print statement
        else if (op == 0x04) {
            char* v = stack[--sp];
            fprintf(out, "print(%s);\n", v);
        }
        // HALT
        else if (op == 0xFF) {
            break;
        }
    }

    fclose(out);
    free(bytecode);

    for (uint8_t i = 0; i < const_count; i++)
        free(constants[i].data);

    printf("Decompiled OK -> decompiled_source.txt\n");
    return 0;
}
