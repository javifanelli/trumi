#ifndef TRUMI_CONFIG_H
#define TRUMI_CONFIG_H

#include "display.h"

// ─── Como es Trumi (¡jueguen con estos numeros!) ─────────────────────────────
// Los pines de la placa estan en components/board_config/include/board_config.h

// Colores: COLOR(rojo, verde, azul), cada uno de 0 a 255
#define COLOR_FONDO     COLOR(20, 10, 40)      // violeta muy oscurito
#define COLOR_OJOS      COLOR(170, 140, 255)   // lila
#define COLOR_BRILLO    COLOR(255, 255, 255)   // los brillitos de los ojos
#define COLOR_PESTANAS  COLOR(255, 130, 200)   // rosa (como las de mama Flufi)
#define COLOR_CACHETES  COLOR(255, 120, 170)   // rosados
#define COLOR_BOCA      COLOR(255, 90, 140)
#define COLOR_BOCA_ADENTRO COLOR(110, 20, 60)   // adentro de la boca, cuando habla
#define COLOR_TEXTO     COLOR(255, 200, 235)

// Brillo de la pantalla, de 0 a 100 %
#define BRILLO_PANTALLA 70

// Volumen del parlante, de 0 a 100 %
#define VOLUMEN         70

// ─── Saludo al prenderse ─────────────────────────────────────────────────────
#define MENSAJE_ARRANQUE        "¡Hola!\nSoy\nTrumi"
#define MENSAJE_ARRANQUE_TAMANO 7        // grandote
#define MENSAJE_ARRANQUE_MS     2500
#define SALUDO                  "¡Hola Sofi!"

// ─── Los oidos ───────────────────────────────────────────────────────────────
#define MANTENER_PARA_ESCUCHAR_MS 700    // dejando el dedo apoyado este tiempo, se pone a escuchar
#define GRABAR_MAX_S              8      // lo mas largo que escucha de una vez (segundos)
#define UMBRAL_RUIDO              0.35f  // que tan fuerte tiene que ser un ruido para que pregunte (0 a 1)
#define ESPERA_ENTRE_RUIDOS_MS    15000  // despues de preguntar "¿Me hablaste?", cuanto espera para volver a hacerlo

// ─── Juegos ──────────────────────────────────────────────────────────────────
// Descanso entre un juego y otro (se elige un valor al azar en este rango)
#define DESCANSO_MIN_MS  300
#define DESCANSO_MAX_MS  1500

#endif // TRUMI_CONFIG_H
