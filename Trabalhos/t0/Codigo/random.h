#ifndef RANDOM_H
#define RANDOM_H

#include "err.h"


// Função que será usada para leitura do dispositivo aleatório
err_t random_leitura(void *disp, int id, int *pvalor); 


#endif