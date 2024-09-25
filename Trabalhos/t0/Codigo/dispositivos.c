#include "dispositivos.h"

#include <stdlib.h>
#include <time.h>
#include "err.h"

err_t random_leitura(void *disp, int id, int *pvalor) {
    static int inicializado = 0; 
    if (!inicializado) {
        srand(time(NULL));
        inicializado = 1;
    }
    if (pvalor == NULL) {
        return ERR_DISP_INV; 
    }

    // Gera um número aleatório e o atribui ao ponteiro pvalor
    *pvalor = rand()%25; 

    return ERR_OK; 
}