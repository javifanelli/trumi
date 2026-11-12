#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// El parlante y los microfonos de Trumi.
//   Parlante: conversor ES8311 + amplificador HT6872.
//   Microfonos: dos microfonos + conversor ES7210 (uno a cada lado del canal).
// El audio es de 16 bits, mono, a AUDIO_MUESTRAS_POR_SEGUNDO muestras por
// segundo. Suena "de fondo" (en otra tarea): mientras suena, la cara se puede
// seguir moviendo. Hay que llamar a placa_iniciar() antes.

#define AUDIO_MUESTRAS_POR_SEGUNDO 16000

void audio_iniciar(void);

// Volumen del parlante, de 0 (mudo) a 100 (al maximo)
void audio_volumen(int porcentaje);

// Pone en la fila para sonar un audio crudo (16 bits, mono, 16 kHz). No espera
// a que termine: vuelve enseguida. "pcm" tiene que seguir existiendo hasta que
// termine de sonar (por ejemplo, un audio guardado en el programa).
void audio_reproducir(const void *pcm, size_t bytes);

// Una nota: frecuencia en Hz (0 = silencio) y duracion en milisegundos
typedef struct { int hz; int ms; } audio_nota_t;

// Pone en la fila una melodia (las notas se copian: no hace falta que sigan existiendo)
void audio_melodia(const audio_nota_t *notas, int cantidad);

bool  audio_sonando(void);       // true mientras suena algo (o hay algo en la fila)
void  audio_esperar(void);       // espera a que termine todo lo que esta en la fila
float audio_nivel(void);         // que tan fuerte suena ahora, de 0 a 1 (para mover la boca)

// ─── Microfonos ──────────────────────────────────────────────────────────────
// Los microfonos escuchan todo el tiempo (si el ES7210 contesta).

bool  audio_mic_listo(void);     // true si los microfonos andan
float audio_mic_nivel(void);     // que tan fuerte se escucha ahora, de 0 a 1

// Empieza a grabar en "destino" (hasta "max_muestras" muestras de 16 bits, mono,
// 16 kHz). Si ya estaba grabando, vuelve a empezar.
void   audio_grabar_empezar(int16_t *destino, size_t max_muestras);
bool   audio_grabar_lleno(void);       // true si ya se lleno el lugar
size_t audio_grabar_terminar(void);    // deja de grabar y devuelve cuantas muestras grabo

#endif // AUDIO_H
