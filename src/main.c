#include <stdio.h>
#include <stdlib.h>

#include "repl.h"
#include "variable.h"

int main(void) {
    repl_run();
    free_all_variables();
    printf("Goodbye!\n");
    return 0;
}