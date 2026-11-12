#ifndef CEREBRO_H
#define CEREBRO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// El cerebro de Trumi vive en la Raspberry (ver el repo "cerebro"). Este
// componente le manda lo que Trumi escucho y recibe la respuesta: hablada (audio
// listo para el parlante) y escrita (para la pantalla).
// La direccion y la clave estan en cerebro_secretos.h (que NO se sube al repo).
// Hay que tener WiFi.

#define CEREBRO_MAX_TEXTO 320

typedef struct {
    bool     ok;                              // false si algo fallo (ver "error")
    const int16_t *audio;                     // la respuesta hablada (16 bits, mono, 16 kHz)
    size_t   muestras;
    char     escuche[CEREBRO_MAX_TEXTO];      // lo que entendio
    char     pantalla[CEREBRO_MAX_TEXTO];     // lo que contesta, sin tildes (para la pantalla)
    char     error[64];
} cerebro_respuesta_t;

// Prepara el lugar para las respuestas (en la PSRAM)
void cerebro_iniciar(void);

// true si hay datos del cerebro (cerebro_secretos.h)
bool cerebro_configurado(void);

// true si se puede charlar: hay WiFi y el cerebro contesto al prenderse (y no
// fallo despues). Si es false, Trumi anda sola (juega al lorito).
bool cerebro_disponible(void);

// Le manda lo que escucho (audio de 16 bits, mono, 16 kHz) y vuelve enseguida:
// el cerebro piensa "de fondo". "audio" tiene que seguir existiendo hasta que termine.
// Devuelve false si no pudo empezar (sin datos, o ya estaba pensando otra cosa).
bool cerebro_preguntar(const int16_t *audio, size_t muestras);

// true cuando ya contesto (o fallo)
bool cerebro_listo(void);

// La respuesta (valida hasta la proxima pregunta)
const cerebro_respuesta_t *cerebro_respuesta(void);

#endif // CEREBRO_H
