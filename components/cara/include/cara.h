#ifndef CARA_H
#define CARA_H

#include <stdbool.h>
#include "display.h"

// La cara de Trumi: los ojos de papa Pitri con las pestañas de mama Flufi,
// brillitos en los ojos, cachetes rosados y una sonrisa.

typedef struct {
    color_t fondo, ojos, brillo, pestanas, cachetes, boca, boca_adentro;
} cara_colores_t;

// Dibuja la cara tranquila, en el centro
void cara_iniciar(const cara_colores_t *colores);

// "funcion" se consulta en cada cuadro: si devuelve true (por ejemplo, la estan
// tocando), lo que este haciendo la cara se corta enseguida
void cara_set_interrupcion(bool (*funcion)(void));

void cara_parpadear(void);

// Elige un juego al azar (mirar, pasear, parpadear, ponerse contenta...) y lo hace
void cara_jugar(void);

// Mueve los ojos para que miren hacia ese punto de la pantalla
void cara_mirar_hacia(int x, int y, int ms);

// Se pone contenta (ojitos felices, sonrisa grande y saltitos) y muestra "texto"
// abajo (NULL = sin texto)
void cara_contenta(const char *texto, int ms);

// Vuelve a mirar al frente, tranquila
void cara_tranquila(void);

// ─── Hablar ──────────────────────────────────────────────────────────────────
// "nivel" dice que tan fuerte esta hablando (0 a 1). Mientras habla, la boca se
// abre y se cierra con ese nivel (en todos los dibujos de la cara).
void cara_set_voz(float (*nivel)(void));

// Sigue dibujando la cara (con la boca moviendose) mientras "sonando" diga true
void cara_mientras_suena(bool (*sonando)(void));

// ─── Escuchar ────────────────────────────────────────────────────────────────
// "nivel" dice que tan fuerte se escucha (0 a 1). Mientras esta escuchando,
// a los costados aparecen ondas de sonido que se mueven con ese nivel.
void cara_set_oido(float (*nivel)(void));

void cara_escuchar(const char *texto);   // pone cara de escuchar (ojos atentos, ondas) y muestra "texto"
void cara_un_cuadro(void);               // dibuja un cuadro (para ir mostrando las ondas mientras escucha)
void cara_dejar_de_escuchar(void);       // vuelve a la cara de siempre

#endif // CARA_H
