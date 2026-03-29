#include <stdio.h>
#include <stdint.h>

int main() {
    // Open file for writing
    FILE* f = fopen("concat_example.cynb", "wb");
    if (!f) return 1;

    // --- Header (11 bytes total) ---
    uint8_t header[] = {
        'C','Y','N','B',        // Magic number: identifies this as Cynex bytecode
        0x02,                   // Version: 2 (increments with changes)
        0xBF,                   // Flags: 0xBF = unsigned, 0xEC would be signed
        0x0F,0x00,0x00,0x00,    // Code size: 15 bytes of bytecode following
        0x02                    // ConstCount: 2 constants in the pool
    };
    fwrite(header, 1, sizeof(header), f);

    // --- Constant pool ---
    // Format per constant: [Type][Length][Data...]
    uint8_t constant_pool[] = {
        0x02, 0x06, 'H','e','l','l','o',' ',  // Constant 0: Type=string(0x02), Length=6, Data="Hello "
        0x02, 0x06, 'W','o','r','l','d','!'   // Constant 1: Type=string(0x02), Length=6, Data="World!"
    };
    fwrite(constant_pool, 1, sizeof(constant_pool), f);

    // --- Bytecode ---
    // Opcodes:
    // 0x01 = LOAD_CONST <idx>    ; push constant at index onto stack
    // 0x02 = STORE_VAR <idx>     ; pop stack into variable index
    // 0x03 = LOAD_VAR <idx>      ; push variable at index onto stack
    // 0x05 = CONCAT              ; pop top 2 values, concatenate, push result
    // 0x04 = PRINT               ; pop top of stack and print
    // 0xFF = HALT                ; stop execution
    uint8_t bytecode[] = {
        0x01, 0x00, // LOAD_CONST 0 -> push "Hello "
        0x02, 0x00, // STORE_VAR 0 -> store in variable 0
        0x01, 0x01, // LOAD_CONST 1 -> push "World!"
        0x02, 0x01, // STORE_VAR 1 -> store in variable 1
        0x03, 0x00, // LOAD_VAR 0 -> push var0 onto stack
        0x03, 0x01, // LOAD_VAR 1 -> push var1 onto stack
        0x05,       // CONCAT -> var0 .. var1
        0x04,       // PRINT -> print top of stack
        0xFF        // HALT -> stop execution
    };
    fwrite(bytecode, 1, sizeof(bytecode), f);

    // Close the file
    fclose(f);
    return 0;
}
