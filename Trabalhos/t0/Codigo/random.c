#include "random.h"

#include <stdlib.h>
#include <time.h>
#include "err.h"

err_t random_leitura(void *disp, int id, int *pvalor) {
    if (pvalor == NULL) {
        return ERR_DISP_INV; 
    }
    
    *pvalor = rand()%256; 

    return ERR_OK; 
}