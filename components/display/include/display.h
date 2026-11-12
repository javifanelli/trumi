#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

// Pantalla a color de 3,5" (320 x 480, ST7796), igual de facil de usar que la de
// Flufi: se dibuja en una "hoja" en memoria (en la PSRAM) y despues
// display_flush() la manda entera a la pantalla.
//
// Antes de display_init() hay que llamar a placa_iniciar() (hace el reset de la
// pantalla). La luz de fondo se prende con placa_brillo().

// ─── Colores ─────────────────────────────────────────────────────────────────
// Cada color es un numero de 16 bits (RGB565): 5 bits de rojo, 6 de verde y 5 de
// azul. COLOR(r, g, b) lo arma a partir de rojo, verde y azul de 0 a 255.
typedef uint16_t color_t;
#define COLOR(r, g, b)  ((color_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

#define NEGRO           COLOR(0, 0, 0)
#define BLANCO          COLOR(255, 255, 255)

// ─── Funciones ───────────────────────────────────────────────────────────────
void display_init(void);
void display_clear(color_t color);    // pinta toda la hoja de un color
void display_flush(void);             // manda la hoja a la pantalla (espera a que termine)

void display_set_pixel(int x, int y, color_t color);
void display_fill_rect(int x, int y, int w, int h, color_t color);
// Rectangulo relleno con esquinas redondeadas de radio r
void display_fill_round_rect(int x, int y, int w, int h, int r, color_t color);
// Circulo relleno con centro (cx, cy) y radio r
void display_fill_circle(int cx, int cy, int r, color_t color);
// Ovalo relleno con centro (cx, cy), medio ancho rx y medio alto ry
void display_fill_ellipse(int cx, int cy, int rx, int ry, color_t color);
// Triangulo relleno con esquinas en (x0,y0), (x1,y1), (x2,y2)
void display_fill_triangle(int x0, int y0, int x1, int y1, int x2, int y2, color_t color);
// Linea recta de (x0,y0) a (x1,y1), con trazo de "grosor" pixeles
void display_draw_line(int x0, int y0, int x1, int y1, int grosor, color_t color);

// ─── Texto ───────────────────────────────────────────────────────────────────
// Letras de 5x7 pixeles. "escala" las agranda: 1 = chiquitas, 2 = el doble...
// Acepta '\n' para cambiar de renglon y los signos ¡ ¿ °.
void display_draw_text(int x, int y, const char *texto, int escala, color_t color);
// Centrado de izquierda a derecha, empezando a la altura y (y = -1: centrado tambien de arriba a abajo)
void display_draw_text_centered(const char *texto, int y, int escala, color_t color);
int  display_text_width(const char *texto, int escala);
int  display_text_height(const char *texto, int escala);

#endif // DISPLAY_H
