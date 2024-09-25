#include "dispositivos.h"

#include <stdlib.h>
#include <time.h>


int dispositivo_random_leitura(void *controladora, int id, void *buffer, int bytes) {
    if (bytes < sizeof(int)) {
        return -1; // Erro: espaço insuficiente no buffer.
    }

    static int inicializado = 0;
    if (!inicializado) {
        srand(time(NULL));
        inicializado = 1;
    }

    int numero_aleatorio = rand(); 
    *(int *)buffer = numero_aleatorio; // Armazena o número aleatório no buffer.

    // Armazena o valor no dispositivo associado
    dispositivo_random_t *dispositivo = (dispositivo_random_t *)controladora;
    dispositivo->ultimo_valor_aleatorio = numero_aleatorio;

    return sizeof(int); // Retorna o número de bytes lidos.
}
