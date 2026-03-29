#include <stdio.h>
#include <stdint.h>

int main() {
    FILE* f = fopen("hello.cynb", "wb");

    // Header
    uint8_t header[] = { 'C','Y','N','B', 0x01, 0x00, 0x03,0x00,0x00,0x00 };
    fwrite(header, 1, sizeof(header), f);

    // Constant pool
    uint8_t constant_pool[] = { 0x01, 0x0B, 'H','e','l','l','o',' ','W','o','r','l','d' };
    fwrite(constant_pool, 1, sizeof(constant_pool), f);

    // Bytecode
    uint8_t bytecode[] = { 0x01, 0x00, 0x02, 0xFF };
    fwrite(bytecode, 1, sizeof(bytecode), f);

    fclose(f);
    return 0;
}