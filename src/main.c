// main.c
#include <stdio.h>
#include <stdlib.h>
#include "repl.h"
#include "variable.h"
#include "platform.h"
#include "CYNB/bytecode.h"

int main(void)
{
    printf("Cynex v0.16.2 - EARLY BYTECODE TESTING\n\n");

    /* Bytecode test */
    FILE* test_file = fopen("concat_example.cynb", "rb");
    if (test_file) {
        fclose(test_file);
        printf("Found concat_example.cynb, loading...\n");

        BytecodeChunk chunk = { 0 };
        if (load_cynb("concat_example.cynb", &chunk)) {
            printf("Bytecode loaded successfully. Running...\n");
            run_cynb(&chunk);
            printf("Bytecode run finished. Freeing chunk...\n");
            free_cynb(&chunk);
            printf("Chunk freed. Moving to REPL...\n\n");
        }
        else {
            printf("Failed to load bytecode\n");
        }
    }
    else {
        printf("No concat_example.cynb found, skipping test.\n\n");
    }

    printf("Starting REPL...\n");
    repl_run();

    printf("REPL finished. Cleaning up variables...\n");
    free_all_variables();

    printf("Goodbye!\n");
    cynex_sleep(1000);
    return 0;
}