// CYNB/bytecode.h
#ifndef CYNEX_BYTECODE_H
#define CYNEX_BYTECODE_H

#include <stdint.h>      // for uint8_t and uint32_t
#include "value.h"    // for Value type

#define CYN_MAGIC     "CYNB"
#define CYN_VERSION   0x02

typedef enum {
    OP_LOAD_CONST = 0x01,
    OP_STORE_VAR = 0x02,
    OP_LOAD_VAR = 0x03,
    OP_CONCAT = 0x05,
    OP_PRINT = 0x04,
    OP_HALT = 0xFF
} Opcode;

typedef struct {
    uint8_t* code;
    uint32_t code_size;

    Value* constants;
    uint32_t const_count;
} BytecodeChunk;

int  load_cynb(const char* filename, BytecodeChunk* chunk);
void free_cynb(BytecodeChunk* chunk);
void run_cynb(const BytecodeChunk* chunk);

#endif /* CYNEX_BYTECODE_H */
