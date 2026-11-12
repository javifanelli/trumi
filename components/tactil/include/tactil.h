#ifndef TACTIL_H
#define TACTIL_H

#include <stdbool.h>

// La pantalla tactil (FT6336U): dice si la estan tocando y donde.
// Las coordenadas son las mismas que las de la pantalla (x de 0 a 319, y de 0 a 479).
// Hay que llamar a placa_iniciar() antes (usa su bus I2C).

void tactil_iniciar(void);

// true si en este momento hay un dedo apoyado (y copia donde)
bool tactil_tocando(int *x, int *y);

// true si apoyaron el dedo desde la ultima vez que se pregunto (una vez por
// cada toque), y copia donde fue
bool tactil_se_toco(int *x, int *y);

// true si hay un toque esperando, SIN consumirlo (tactil_se_toco lo va a seguir viendo)
bool tactil_hay_toque_nuevo(void);

#endif // TACTIL_H
