// CYNB/vm.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "CYNB/bytecode.h"
#include "platform.h"
#include "variable.h"

static Value stack[256];
static int sp = 0;

static void push(Value v) {
    if (sp >= 256) {
        fprintf(stderr, "Cynex VM: Stack overflow\n");
        return;
    }
    stack[sp++] = v;
}

static Value pop(void) {
    if (sp <= 0) {
        fprintf(stderr, "Cynex VM: Stack underflow\n");
        return make_number(0.0);
    }
    return stack[--sp];
}

/* LOAD */
int load_cynb(const char* filename, BytecodeChunk* chunk)
{
    FILE* f = fopen(filename, "rb");
    if (!f) {
        perror("Failed to open .cynb file");
        return 0;
    }

    char magic[4];
    uint8_t version, flags, const_count;
    uint32_t code_size;

    fread(magic, 1, 4, f);
    fread(&version, 1, 1, f);
    fread(&flags, 1, 1, f);
    fread(&code_size, 4, 1, f);
    fread(&const_count, 1, 1, f);

    if (memcmp(magic, CYN_MAGIC, 4) != 0 || version != CYN_VERSION) {
        fprintf(stderr, "Error: Invalid or unsupported Cynex bytecode file\n");
        fclose(f);
        return 0;
    }

    chunk->code = malloc(code_size);
    chunk->constants = malloc(const_count * sizeof(Value));
    chunk->code_size = code_size;
    chunk->const_count = const_count;

    if (!chunk->code || !chunk->constants) {
        fprintf(stderr, "Cynex VM: Out of memory\n");
        fclose(f);
        return 0;
    }

    /* Read constant pool */
    for (uint32_t i = 0; i < const_count; i++) {
        uint8_t type, len;
        fread(&type, 1, 1, f);
        fread(&len, 1, 1, f);

        char* data = malloc(len + 1);
        if (!data) {
            fprintf(stderr, "Cynex VM: Out of memory reading constant\n");
            fclose(f);
            return 0;
        }

        fread(data, 1, len, f);
        data[len] = '\0';

        chunk->constants[i] = (type == 0x02) ? make_string(data) : make_number(0.0);
        free(data);
    }

    fread(chunk->code, 1, code_size, f);
    fclose(f);
    return 1;
}

/* FREE (simplified for now) */
void free_cynb(BytecodeChunk* chunk)
{
    if (chunk->code) {
        free(chunk->code);
        chunk->code = NULL;
    }
    if (chunk->constants) {
        // We skip freeing string data here to avoid double-free in early version
        free(chunk->constants);
        chunk->constants = NULL;
    }
}

/* RUN */
void run_cynb(const BytecodeChunk* chunk)
{
    sp = 0;
    size_t pc = 0;

    while (pc < chunk->code_size) {
        uint8_t op = chunk->code[pc++];

        switch (op) {
        case OP_LOAD_CONST: {
            uint8_t idx = chunk->code[pc++];
            if (idx < chunk->const_count) {
                push(chunk->constants[idx]);
            }
            break;
        }

        case OP_STORE_VAR: {
            uint8_t idx = chunk->code[pc++];
            Value v = pop();
            if (idx < MAX_VARS) {
                free_value(&vars[idx].value);
                vars[idx].value = v;        // take ownership
                vars[idx].used = 1;
            }
            else {
                free_value(&v);
            }
            break;
        }

        case OP_LOAD_VAR: {
            uint8_t idx = chunk->code[pc++];
            if (idx < MAX_VARS && vars[idx].used) {
                push(vars[idx].value);
            }
            else {
                push(make_number(0.0));
            }
            break;
        }

        case OP_CONCAT: {
            Value b = pop();
            Value a = pop();

            char* sa = value_to_cstring(&a);
            char* sb = value_to_cstring(&b);

            size_t len = (sa ? strlen(sa) : 0) + (sb ? strlen(sb) : 0) + 1;
            char* result = malloc(len);
            if (result) {
                strcpy(result, sa ? sa : "");
                if (sb) strcat(result, sb);
                push(make_string(result));
                free(result);
            }
            else {
                push(make_string(""));
            }

            free(sa);
            free(sb);
            free_value(&a);
            free_value(&b);
            break;
        }

        case OP_PRINT: {
            Value v = pop();
            char* s = value_to_cstring(&v);
            if (s) {
                printf("%s\n", s);
                free(s);
            }
            free_value(&v);
            break;
        }

        case OP_HALT:
            return;

        default:
            fprintf(stderr, "Cynex VM: Unknown opcode 0x%02X\n", op);
            return;
        }
    }
}
