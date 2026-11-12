#include "display.h"
#include "board_config.h"
#include "font5x7.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_lcd_io_i80.h"
#include "esp_lcd_panel_io.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "DISPLAY";

/*
 * Como funciona la pantalla ST7796:
 *   - Se le habla por un bus "paralelo" de 8 cables de datos (D0..D7): manda un
 *     byte entero por vez, mucho mas rapido que el SPI de Flufi.
 *   - El cable RS dice si lo que mandamos es un COMANDO o DATOS, y el cable WR
 *     marca el ritmo (cada "tic" es un byte).
 *   - Cada pixel son 2 bytes de color (RGB565), y la pantalla los quiere con el
 *     byte "grande" primero. Por eso en la hoja los guardamos ya dados vuelta.
 *   - La hoja entera ocupa 320 x 480 x 2 = 300 KB: va en la PSRAM (los 8 MB
 *     extra de la placa). Se manda por DMA: la placa la copia sola, sin ocupar
 *     al procesador.
 */

static esp_lcd_panel_io_handle_t io;
static uint16_t *hoja;                  // LCD_H_RES x LCD_V_RES pixeles, listos para mandar
static SemaphoreHandle_t termino;       // avisa cuando termino de mandar la hoja

// ─── Las dos versiones de la pantalla ────────────────────────────────────────
// La placa viene con una de dos pantallas parecidas, que se prenden con pasos
// un poco distintos. Segun el fabricante, se distinguen asi: antes de arrancar,
// los pines D0 y D3 leen 1 y 1 en una version, y otra cosa en la otra.

typedef struct {
    uint8_t cmd;
    uint8_t datos[14];
    uint8_t cantidad;
    uint16_t espera_ms;
} paso_t;

#define FIN_PASOS { 0, { 0 }, 0xFF, 0 }

static const paso_t pasos_v1[] = {
    { 0x11, { 0 }, 0, 120 },                                          // despertarse
    { 0x36, { LCD_MADCTL }, 1, 0 },                                   // orientacion
    { 0x3A, { 0x55 }, 1, 0 },                                         // 16 bits por pixel
    { 0xF0, { 0xC3 }, 1, 0 }, { 0xF0, { 0x96 }, 1, 0 },               // habilitar ajustes
    { 0xB4, { 0x01 }, 1, 0 }, { 0xB5, { 0x1E }, 1, 0 }, { 0xB7, { 0xC6 }, 1, 0 },
    { 0xB9, { 0x02, 0xE0 }, 2, 0 }, { 0xC0, { 0x80, 0x16 }, 2, 0 },
    { 0xC1, { 0x19 }, 1, 0 }, { 0xC2, { 0xA7 }, 1, 0 }, { 0xC5, { 0x16 }, 1, 0 },
    { 0xE8, { 0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33 }, 8, 0 },
    { 0xE0, { 0xF0, 0x07, 0x0D, 0x04, 0x05, 0x14, 0x36, 0x54, 0x4C, 0x38, 0x13, 0x14, 0x2E, 0x34 }, 14, 0 },
    { 0xE1, { 0xF0, 0x10, 0x14, 0x0E, 0x0C, 0x08, 0x35, 0x44, 0x4C, 0x26, 0x10, 0x12, 0x2C, 0x32 }, 14, 0 },
    { 0xF0, { 0x3C }, 1, 0 }, { 0xF0, { 0x69 }, 1, 120 },             // cerrar ajustes
    { 0x29, { 0 }, 0, 0 },                                            // prenderse
    { 0x21, { 0 }, 0, 0 },                                            // colores invertidos (asi es esta pantalla)
    FIN_PASOS
};

static const paso_t pasos_v2[] = {
    { 0x11, { 0 }, 0, 120 },
    { 0x36, { LCD_MADCTL }, 1, 0 },
    { 0x3A, { 0x55 }, 1, 0 },
    { 0xF0, { 0xC3 }, 1, 0 }, { 0xF0, { 0x96 }, 1, 0 },
    { 0xB1, { 0x90, 0x18 }, 2, 0 }, { 0xB4, { 0x01 }, 1, 0 }, { 0xB7, { 0xC6 }, 1, 0 },
    { 0xB9, { 0x02, 0xE0 }, 2, 0 }, { 0xC0, { 0x00, 0x00 }, 2, 0 }, { 0xC1, { 0x16 }, 1, 0 },
    { 0xC6, { 0x80 }, 1, 0 }, { 0xC2, { 0xA7 }, 1, 0 }, { 0xC5, { 0x00 }, 1, 0 },
    { 0xE8, { 0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33 }, 8, 0 },
    { 0xE0, { 0xF0, 0x09, 0x0F, 0x0D, 0x0D, 0x1C, 0x3D, 0x44, 0x55, 0x39, 0x18, 0x18, 0x36, 0x39 }, 14, 0 },
    { 0xE1, { 0xF0, 0x09, 0x0F, 0x09, 0x08, 0x01, 0x31, 0x33, 0x46, 0x09, 0x13, 0x13, 0x2A, 0x31 }, 14, 0 },
    { 0xF0, { 0x3C }, 1, 0 }, { 0xF0, { 0x69 }, 1, 120 },
    { 0x21, { 0 }, 0, 0 },
    { 0x29, { 0 }, 0, 0 },
    FIN_PASOS
};

// ¿Que version de pantalla tiene? (hay que mirarlo ANTES de armar el bus)
static int version_de_pantalla(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << PIN_LCD_D0) | (1ULL << PIN_LCD_D3),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&cfg);
    vTaskDelay(pdMS_TO_TICKS(2));
    bool d0 = gpio_get_level(PIN_LCD_D0), d3 = gpio_get_level(PIN_LCD_D3);
    gpio_reset_pin(PIN_LCD_D0);
    gpio_reset_pin(PIN_LCD_D3);
    return (d0 && d3) ? 1 : 2;
}

static bool al_terminar(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *ctx)
{
    BaseType_t despertar = pdFALSE;
    xSemaphoreGiveFromISR(termino, &despertar);
    return despertar == pdTRUE;
}

// ─── Arranque ────────────────────────────────────────────────────────────────

void display_init(void)
{
    int version = version_de_pantalla();
    ESP_LOGI(TAG, "Inicializando la pantalla ST7796 (version %d)...", version);

    termino = xSemaphoreCreateBinary();
    const size_t bytes_hoja = LCD_H_RES * LCD_V_RES * sizeof(uint16_t);

    esp_lcd_i80_bus_config_t bus_cfg = {
        .dc_gpio_num        = PIN_LCD_RS,
        .wr_gpio_num        = PIN_LCD_WR,
        .clk_src            = LCD_CLK_SRC_DEFAULT,
        .data_gpio_nums     = { PIN_LCD_D0, PIN_LCD_D1, PIN_LCD_D2, PIN_LCD_D3,
                                PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7 },
        .bus_width          = 8,
        .max_transfer_bytes = bytes_hoja,
        .dma_burst_size     = 64,
    };
    esp_lcd_i80_bus_handle_t bus;
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_cfg, &bus));

    esp_lcd_panel_io_i80_config_t io_cfg = {
        .cs_gpio_num         = -1,          // esta placa no usa CS: la pantalla esta siempre elegida
        .pclk_hz             = LCD_PCLK_HZ,
        .trans_queue_depth   = 4,
        .on_color_trans_done = al_terminar,
        .lcd_cmd_bits        = 8,
        .lcd_param_bits      = 8,
        .dc_levels           = { .dc_idle_level = 0, .dc_cmd_level = 0, .dc_dummy_level = 0, .dc_data_level = 1 },
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(bus, &io_cfg, &io));

    hoja = esp_lcd_i80_alloc_draw_buffer(io, bytes_hoja, MALLOC_CAP_SPIRAM);
    assert(hoja != NULL);

    esp_lcd_panel_io_tx_param(io, 0x01, NULL, 0);                 // reset por software
    vTaskDelay(pdMS_TO_TICKS(120));
    for (const paso_t *p = (version == 1) ? pasos_v1 : pasos_v2; p->cantidad != 0xFF; p++) {
        esp_lcd_panel_io_tx_param(io, p->cmd, p->cantidad ? p->datos : NULL, p->cantidad);
        if (p->espera_ms) vTaskDelay(pdMS_TO_TICKS(p->espera_ms));
    }

    display_clear(NEGRO);
    display_flush();
    ESP_LOGI(TAG, "Pantalla lista");
}

// ─── La hoja ─────────────────────────────────────────────────────────────────

// La pantalla quiere el byte grande primero: lo damos vuelta al guardarlo
static inline uint16_t dar_vuelta(color_t c)
{
    return (uint16_t)((c >> 8) | (c << 8));
}

void display_clear(color_t color)
{
    uint16_t v = dar_vuelta(color);
    for (int i = 0; i < LCD_H_RES * LCD_V_RES; i++) hoja[i] = v;
}

void display_flush(void)
{
    uint8_t columnas[4] = { 0, 0, (LCD_H_RES - 1) >> 8, (LCD_H_RES - 1) & 0xFF };
    uint8_t filas[4]    = { 0, 0, (LCD_V_RES - 1) >> 8, (LCD_V_RES - 1) & 0xFF };
    esp_lcd_panel_io_tx_param(io, 0x2A, columnas, 4);              // CASET: columnas
    esp_lcd_panel_io_tx_param(io, 0x2B, filas, 4);                 // RASET: filas
    esp_lcd_panel_io_tx_color(io, 0x2C, hoja, LCD_H_RES * LCD_V_RES * sizeof(uint16_t));   // RAMWR
    xSemaphoreTake(termino, portMAX_DELAY);    // no tocar la hoja hasta que termine de mandarla
}

// ─── Formas ──────────────────────────────────────────────────────────────────

// Una raya horizontal de x0 a x1 (incluidos) en la fila y, ya recortada a la pantalla
static void linea_horizontal(int x0, int x1, int y, uint16_t v)
{
    if (y < 0 || y >= LCD_V_RES) return;
    if (x0 < 0) x0 = 0;
    if (x1 >= LCD_H_RES) x1 = LCD_H_RES - 1;
    uint16_t *p = &hoja[y * LCD_H_RES + x0];
    for (int x = x0; x <= x1; x++) *p++ = v;
}

void display_set_pixel(int x, int y, color_t color)
{
    if (x < 0 || x >= LCD_H_RES || y < 0 || y >= LCD_V_RES) return;
    hoja[y * LCD_H_RES + x] = dar_vuelta(color);
}

void display_fill_rect(int x, int y, int w, int h, color_t color)
{
    if (w <= 0 || h <= 0) return;
    uint16_t v = dar_vuelta(color);
    for (int j = y; j < y + h; j++) linea_horizontal(x, x + w - 1, j, v);
}

void display_fill_round_rect(int x, int y, int w, int h, int r, color_t color)
{
    if (w <= 0 || h <= 0) return;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    uint16_t v = dar_vuelta(color);

    for (int j = 0; j < h; j++) {
        // Distancia vertical al centro de la esquina mas cercana
        int dy = 0;
        if (j < r)           dy = r - j;
        else if (j >= h - r) dy = j - (h - r - 1);

        // Cuanto "se mete" la fila por la curva de la esquina
        int adentro = 0;
        if (dy > 0) {
            int dx = 0;
            while ((dx + 1) * (dx + 1) + dy * dy <= r * r) dx++;
            adentro = r - dx;
        }
        linea_horizontal(x + adentro, x + w - 1 - adentro, y + j, v);
    }
}

void display_fill_circle(int cx, int cy, int r, color_t color)
{
    display_fill_ellipse(cx, cy, r, r, color);
}

void display_fill_ellipse(int cx, int cy, int rx, int ry, color_t color)
{
    if (rx <= 0 || ry <= 0) return;
    uint16_t v = dar_vuelta(color);
    // Para cada fila, el medio ancho: x^2/rx^2 + y^2/ry^2 <= 1 (sin raices: con enteros)
    long rx2 = (long)rx * rx, ry2 = (long)ry * ry;
    int dx = rx;
    for (int j = 0; j <= ry; j++) {
        while (dx > 0 && (long)dx * dx * ry2 + (long)j * j * rx2 > rx2 * ry2) dx--;
        linea_horizontal(cx - dx, cx + dx, cy + j, v);
        if (j > 0) linea_horizontal(cx - dx, cx + dx, cy - j, v);
    }
}

// Dice de que lado de la linea (a→b) queda el punto p
static int lado_de_linea(int ax, int ay, int bx, int by, int px, int py)
{
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

void display_fill_triangle(int x0, int y0, int x1, int y1, int x2, int y2, color_t color)
{
    int min_x = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
    int max_x = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
    int min_y = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
    int max_y = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
    uint16_t v = dar_vuelta(color);

    // Un punto esta adentro si queda del mismo lado de las tres lineas
    for (int y = min_y; y <= max_y; y++) {
        if (y < 0 || y >= LCD_V_RES) continue;
        for (int x = min_x; x <= max_x; x++) {
            int a = lado_de_linea(x0, y0, x1, y1, x, y);
            int b = lado_de_linea(x1, y1, x2, y2, x, y);
            int c = lado_de_linea(x2, y2, x0, y0, x, y);
            if (((a >= 0 && b >= 0 && c >= 0) || (a <= 0 && b <= 0 && c <= 0)) && x >= 0 && x < LCD_H_RES)
                hoja[y * LCD_H_RES + x] = v;
        }
    }
}

void display_draw_line(int x0, int y0, int x1, int y1, int grosor, color_t color)
{
    int dx = x1 - x0, dy = y1 - y0;
    int pasos = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
    if (pasos == 0) pasos = 1;
    for (int i = 0; i <= pasos; i++) {
        int x = x0 + dx * i / pasos;
        int y = y0 + dy * i / pasos;
        display_fill_rect(x - grosor / 2, y - grosor / 2, grosor, grosor, color);
    }
}

// ─── Texto (igual que en Flufi) ──────────────────────────────────────────────

#define SEPARACION_LETRAS  1   // columnas vacias entre letra y letra
#define SEPARACION_RENGLON 2   // filas vacias entre renglon y renglon

// Caracteres especiales (fuera del ASCII)
#define GLIFO_ABRE_EXCLAMACION 1   // ¡
#define GLIFO_ABRE_PREGUNTA    2   // ¿
#define GLIFO_GRADO            3   // °

static const uint8_t columnas_grado[FONT_ANCHO] = { 0x06, 0x09, 0x09, 0x06, 0x00 };

// Lee el proximo caracter del texto (entiende UTF-8 para ¡ ¿ °) y avanza el puntero
static int proximo_caracter(const char **p)
{
    const unsigned char *s = (const unsigned char *)*p;
    if (*s == 0) return 0;
    if (*s < 0x80) { (*p)++; return *s; }
    if (s[0] == 0xC2 && s[1] == 0xA1) { *p += 2; return GLIFO_ABRE_EXCLAMACION; }
    if (s[0] == 0xC2 && s[1] == 0xBF) { *p += 2; return GLIFO_ABRE_PREGUNTA; }
    if (s[0] == 0xC2 && s[1] == 0xB0) { *p += 2; return GLIFO_GRADO; }

    // Otro caracter UTF-8 que no tenemos: lo salteamos entero y dibujamos '?'
    (*p)++;
    while ((**p & 0xC0) == 0x80) (*p)++;
    return '?';
}

// Las 5 columnas de un caracter (¡ y ¿ son ! y ? dados vuelta)
static void columnas_de(int c, uint8_t col[FONT_ANCHO])
{
    if (c == GLIFO_GRADO) { memcpy(col, columnas_grado, FONT_ANCHO); return; }

    bool dar_vuelta_letra = false;
    if (c == GLIFO_ABRE_EXCLAMACION) { c = '!'; dar_vuelta_letra = true; }
    if (c == GLIFO_ABRE_PREGUNTA)    { c = '?'; dar_vuelta_letra = true; }
    if (c < FONT_PRIMERO || c > FONT_ULTIMO) c = '?';

    for (int i = 0; i < FONT_ANCHO; i++) {
        uint8_t b = font5x7[c - FONT_PRIMERO][i];
        if (dar_vuelta_letra) {
            uint8_t r = 0;
            for (int k = 0; k < FONT_ALTO; k++)
                if (b & (1 << k)) r |= 1 << (FONT_ALTO - 1 - k);
            b = r;
        }
        col[i] = b;
    }
}

static void dibujar_caracter(int x, int y, int c, int escala, color_t color)
{
    uint8_t col[FONT_ANCHO];
    columnas_de(c, col);
    for (int i = 0; i < FONT_ANCHO; i++)
        for (int k = 0; k < FONT_ALTO; k++)
            if (col[i] & (1 << k))
                display_fill_rect(x + i * escala, y + k * escala, escala, escala, color);
}

// Ancho del renglon que empieza en p (hasta '\n' o el final)
static int ancho_renglon(const char *p, int escala)
{
    int letras = 0, c;
    while ((c = proximo_caracter(&p)) != 0 && c != '\n') letras++;
    if (letras == 0) return 0;
    return (letras * (FONT_ANCHO + SEPARACION_LETRAS) - SEPARACION_LETRAS) * escala;
}

int display_text_width(const char *texto, int escala)
{
    int max = 0;
    const char *p = texto;
    while (1) {
        int w = ancho_renglon(p, escala);
        if (w > max) max = w;
        const char *salto = strchr(p, '\n');
        if (!salto) break;
        p = salto + 1;
    }
    return max;
}

int display_text_height(const char *texto, int escala)
{
    int renglones = 1;
    for (const char *p = texto; *p; p++)
        if (*p == '\n') renglones++;
    return (renglones * (FONT_ALTO + SEPARACION_RENGLON) - SEPARACION_RENGLON) * escala;
}

// centro_x < 0: el texto empieza en x. Si no, cada renglon se centra en centro_x.
static void dibujar_texto(int x, int y, const char *texto, int escala, color_t color, int centro_x)
{
    const char *p = texto;
    int alto_renglon = (FONT_ALTO + SEPARACION_RENGLON) * escala;
    int cx = (centro_x >= 0) ? centro_x - ancho_renglon(p, escala) / 2 : x;
    int c;

    while ((c = proximo_caracter(&p)) != 0) {
        if (c == '\n') {
            y += alto_renglon;
            cx = (centro_x >= 0) ? centro_x - ancho_renglon(p, escala) / 2 : x;
            continue;
        }
        dibujar_caracter(cx, y, c, escala, color);
        cx += (FONT_ANCHO + SEPARACION_LETRAS) * escala;
    }
}

void display_draw_text(int x, int y, const char *texto, int escala, color_t color)
{
    dibujar_texto(x, y, texto, escala, color, -1);
}

void display_draw_text_centered(const char *texto, int y, int escala, color_t color)
{
    if (y < 0) y = (LCD_V_RES - display_text_height(texto, escala)) / 2;
    dibujar_texto(0, y, texto, escala, color, LCD_H_RES / 2);
}
