#include <stdio.h>
#include <stdlib.h>  // EXIT_FAILURE
#include "app/app.h"
#include "definitions.h"  // Harmony driv

int main(void) {
    /* Initialize all modules */
    SYS_Initialize(NULL);

    RESET_TH_Clear();

    /* Initialize application logic */
    app_init();

    while (true) {
        /* Run application logic */
        app_task();
    }

    /* Execution should not come here during normal operation */

    return (EXIT_FAILURE);
}
