#include <stdio.h>
#include <stdint.h>

int main() {
    FILE* f = fopen("concat_example.cynb", "wb");
    if (!f) return 1;

    // --- Header ---
    uint8_t header[] = {
        'C','Y','N','B',   // Magic
        0x01,              // Version
        0x00,              // Flags
        0x0A,0x00,0x00,0x00 // Code size = 10 bytes
    };
    fwrite(header, 1, sizeof(header), f);

    // --- Constant pool ---
    uint8_t constant_pool[] = {
        0x02, 0x06, 'H','e','l','l','o',' ',  // "Hello "
        0x02, 0x06, 'W','o','r','l','d','!'   // "World!"
    };
    fwrite(constant_pool, 1, sizeof(constant_pool), f);

    // --- Bytecode ---
    uint8_t bytecode[] = {
        0x01, 0x00, // LOAD_CONST 0
        0x02, 0x00, // STORE_VAR 0 (partial)
        0x01, 0x01, // LOAD_CONST 1
        0x02, 0x01, // STORE_VAR 1 (partialend)
        0x03, 0x00, // LOAD_VAR 0
        0x03, 0x01, // LOAD_VAR 1
        0x05,       // CONCAT top two stack values
        0x04,       // PRINT
        0xFF        // HALT
    };
    fwrite(bytecode, 1, sizeof(bytecode), f);

    fclose(f);
    return 0;
}
