#ifndef PLACA_H
#define PLACA_H

#include <stdbool.h>
#include "driver/i2c_master.h"

// Lo basico de la placa: el bus I2C (por donde se habla con el tactil, el audio
// y el expansor) y el expansor de pines AW9523 (luz de la pantalla, amplificador
// del parlante y reset). Hay que llamar a placa_iniciar() antes que nada.

void placa_iniciar(void);

// El bus I2C compartido (para agregarle otros aparatos: tactil, audio...)
i2c_master_bus_handle_t placa_bus_i2c(void);

// Brillo de la pantalla, de 0 (apagada) a 100 (al maximo)
void placa_brillo(int porcentaje);

// Prende o apaga el amplificador del parlante
void placa_amplificador(bool prendido);

#endif // PLACA_H
