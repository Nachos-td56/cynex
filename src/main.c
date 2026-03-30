#include <stdio.h>
#include <stdlib.h>
#include "platform.h"
#include "repl.h"
#include "variable.h"

int main(void) {
    repl_run();
    free_all_variables();
    printf("Goodbye!\n");
    cynex_sleep(1000);
    return 0;
}