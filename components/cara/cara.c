#include "cara.h"
#include "board_config.h"

#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_random.h"
#include "esp_log.h"

static const char *TAG = "CARA";

#define PI ((float)M_PI)

// ─── Forma de la cara (¡jueguen con estos numeros!) ──────────────────────────
#define OJO_ANCHO        96     // pixeles
#define OJO_ALTO         128
#define OJO_RADIO        40     // que tan redondeadas son las esquinas
#define OJO_SEPARACION   36     // espacio entre los dos ojos
#define OJO_ALTO_CERRADO 8      // grosor de la rayita cuando esta cerrado
#define OJOS_Y           200    // altura del centro de los ojos (la pantalla mide 480)
#define MAX_X            44     // hasta donde se mueven los ojos para los costados
#define MAX_Y            40     // y para arriba y abajo
#define PESTANA_GROSOR   6
#define BOCA_Y           (OJOS_Y + 130)
#define TEXTO_DESDE      372    // la zona del texto de abajo (hasta el final de la pantalla)
#define CUADRO_MS        30     // cada cuanto se dibuja un cuadro de la animacion

// Como esta la cara en este momento
typedef struct {
    float x, y;          // adonde mira (corrimiento desde el centro)
    float abre_izq, abre_der;   // 0 = cerrado, 1 = abierto
    float feliz;         // 0 a 1: ojitos felices ^ ^ y sonrisa grande
    float escucha;       // 0 a 1: cara de escuchar (ojos un poco mas grandes y ondas)
} cara_t;

static const cara_t CARA_NORMAL = { .abre_izq = 1, .abre_der = 1 };
static cara_t cara;
static cara_colores_t col;
static const char *texto_abajo = NULL;
static float (*nivel_voz)(void) = NULL;   // que tan fuerte esta hablando
static float boca_abierta = 0;           // que tan abierta esta la boca (0 a 1)
static float (*nivel_oido)(void) = NULL;  // que tan fuerte se escucha

// ─── Cortar lo que esta haciendo ─────────────────────────────────────────────

static bool (*hay_que_cortar)(void) = NULL;
static bool cortado = false;

void cara_set_interrupcion(bool (*funcion)(void)) { hay_que_cortar = funcion; }

static bool cortar_ahora(void)
{
    if (!cortado && hay_que_cortar != NULL && hay_que_cortar()) cortado = true;
    return cortado;
}

static void esperar(int ms)
{
    for (int t = 0; t < ms && !cortar_ahora(); t += 20) vTaskDelay(pdMS_TO_TICKS(20));
}

static int azar(int min, int max)
{
    return min + (int)(esp_random() % (uint32_t)(max - min + 1));
}

// ─── Dibujar ─────────────────────────────────────────────────────────────────

static void arco(float cx, float cy, float r, float desde, float hasta, int grosor, color_t c)
{
    for (float a = desde; a <= hasta; a += 0.02f)
        display_fill_rect((int)(cx + r * cosf(a)) - grosor / 2, (int)(cy + r * sinf(a)) - grosor / 2, grosor, grosor, c);
}

// Las pestañas: tres rayitas que salen del borde de arriba del ojo, del lado de
// afuera (como las de mama Flufi). Con el ojo cerrado apuntan para abajo.
static void pestanas(float cx, float arriba, int lado, float abre)
{
    static const int p[3][4] = { { 40, 8, 58, -12 }, { 24, 2, 34, -20 }, { 6, 0, 10, -24 } };
    int hacia = (abre < 0.3f) ? -1 : 1;
    for (int k = 0; k < 3; k++)
        display_draw_line((int)(cx + lado * p[k][0]), (int)(arriba + p[k][1] * hacia),
                          (int)(cx + lado * p[k][2]), (int)(arriba + p[k][3] * hacia), PESTANA_GROSOR, col.pestanas);
}

// lado: -1 = ojo izquierdo, +1 = ojo derecho
static void ojo(float cx, float cy, float abre, int lado)
{
    float alto = OJO_ALTO * (1 + 0.12f * cara.escucha);         // escuchando: ojos mas grandes
    int h = (int)(OJO_ALTO_CERRADO + (alto - OJO_ALTO_CERRADO) * abre);
    int x = (int)(cx - OJO_ANCHO / 2.0f), y = (int)(cy - h / 2.0f);
    display_fill_round_rect(x, y, OJO_ANCHO, h, OJO_RADIO, col.ojos);

    if (abre > 0.5f) {                                   // brillitos (miran hacia el mismo lado)
        int bx = (int)(cx - 16 + cara.x * 0.15f), by = (int)(y + h * 0.28f);
        display_fill_circle(bx, by, 14, col.brillo);
        display_fill_circle(bx + 22, by + 22, 6, col.brillo);
    }
    if (cara.feliz > 0.01f && h > OJO_ALTO_CERRADO) {     // ojitos felices: una "loma" tapa la parte de abajo
        int tapa = (int)(h * 0.6f * cara.feliz);
        display_fill_round_rect(x - 8, y + h - tapa, OJO_ANCHO + 16, h, (OJO_ANCHO + 16) / 2, col.fondo);
    }
    pestanas(cx, y, lado, abre);
}

static void dibujar(void)
{
    float cy = OJOS_Y + cara.y;
    float cx_izq = LCD_H_RES / 2.0f - OJO_SEPARACION / 2.0f - OJO_ANCHO / 2.0f + cara.x;
    float cx_der = LCD_H_RES / 2.0f + OJO_SEPARACION / 2.0f + OJO_ANCHO / 2.0f + cara.x;
    float mx = LCD_H_RES / 2.0f + cara.x * 0.6f;

    display_clear(col.fondo);

    // Escuchando: ondas de sonido a los costados (mas ondas cuanto mas fuerte se escucha)
    if (cara.escucha > 0.5f) {
        float oido = (nivel_oido != NULL) ? nivel_oido() : 0;
        int ondas = 1 + (int)(oido * 8);
        if (ondas > 3) ondas = 3;
        for (int k = 0; k < ondas; k++) {
            arco(-30, cy, 44 + 16 * k, -0.33f * PI, 0.33f * PI, 6, col.pestanas);
            arco(LCD_H_RES + 30, cy, 44 + 16 * k, 0.67f * PI, 1.33f * PI, 6, col.pestanas);
        }
    }

    ojo(cx_izq, cy, cara.abre_izq, -1);
    ojo(cx_der, cy, cara.abre_der, +1);

    // Cachetes rosados, abajo de los ojos
    display_fill_ellipse((int)(cx_izq - 22), (int)(cy + 92), 26, 14, col.cachetes);
    display_fill_ellipse((int)(cx_der + 22), (int)(cy + 92), 26, 14, col.cachetes);

    // La boca: si esta hablando, se abre y se cierra con la voz. Si no, una
    // sonrisa que crece cuando esta contenta.
    float voz = (nivel_voz != NULL) ? nivel_voz() * 1.5f : 0;
    if (voz > 1) voz = 1;
    boca_abierta += (voz - boca_abierta) * 0.6f;        // que no salte de golpe
    float by = BOCA_Y + cara.y * 0.6f;
    if (boca_abierta > 0.04f) {
        int rx = (int)(22 + 6 * cara.feliz), ry = (int)(5 + 30 * boca_abierta);
        display_fill_ellipse((int)mx, (int)by, rx, ry, col.boca);
        if (ry > 9) display_fill_ellipse((int)mx, (int)by + 1, rx - 7, ry - 7, col.boca_adentro);
    } else {
        float r = 26 + 18 * cara.feliz;
        arco(mx, by - r * 0.6f, r, 0.18f * PI, 0.82f * PI, 8, col.boca);
    }

    if (texto_abajo != NULL) {
        // La letra mas grande que entre en la zona de abajo (las respuestas largas van mas chicas)
        int alto_zona = LCD_V_RES - TEXTO_DESDE - 6, escala = 4;
        while (escala > 1 && (display_text_width(texto_abajo, escala) > LCD_H_RES - 12 ||
                              display_text_height(texto_abajo, escala) > alto_zona))
            escala--;
        int y = TEXTO_DESDE + (alto_zona - display_text_height(texto_abajo, escala)) / 2;
        display_draw_text_centered(texto_abajo, y, escala, col.pestanas);
    }
    display_flush();
}

// ─── Animar ──────────────────────────────────────────────────────────────────

static float mezclar(float a, float b, float t) { return a + (b - a) * t; }

// Pasa suavemente de la cara actual a "destino" en "ms" milisegundos
static void animar(cara_t destino, int ms)
{
    if (cortar_ahora()) return;
    cara_t inicio = cara;
    int pasos = ms / CUADRO_MS;
    if (pasos < 1) pasos = 1;

    TickType_t ultimo = xTaskGetTickCount();
    for (int i = 1; i <= pasos; i++) {
        float t = (float)i / pasos;
        t = t * t * (3 - 2 * t);                 // arranca y frena despacito
        cara.x        = mezclar(inicio.x,        destino.x,        t);
        cara.y        = mezclar(inicio.y,        destino.y,        t);
        cara.abre_izq = mezclar(inicio.abre_izq, destino.abre_izq, t);
        cara.abre_der = mezclar(inicio.abre_der, destino.abre_der, t);
        cara.feliz    = mezclar(inicio.feliz,    destino.feliz,    t);
        cara.escucha  = mezclar(inicio.escucha,  destino.escucha,  t);
        dibujar();
        if (cortar_ahora()) return;
        // Si dibujar tardo mas que un cuadro, igual descansa un tick (que respiren las otras tareas)
        if (xTaskDelayUntil(&ultimo, pdMS_TO_TICKS(CUADRO_MS)) == pdFALSE) {
            vTaskDelay(1);
            ultimo = xTaskGetTickCount();
        }
    }
}

static void mover_a(float x, float y, int ms)
{
    cara_t d = cara;
    d.x = x;
    d.y = y;
    animar(d, ms);
}

static void cerrar_y_abrir(bool izq, bool der, int cerrado_ms)
{
    cara_t abierta = cara, cerrada = cara;
    if (izq) cerrada.abre_izq = 0;
    if (der) cerrada.abre_der = 0;
    animar(cerrada, 80);
    esperar(cerrado_ms);
    animar(abierta, 120);
}

// ─── Juegos ──────────────────────────────────────────────────────────────────

static void juego_mirar(void)
{
    mover_a(azar(-MAX_X, MAX_X), azar(-MAX_Y, MAX_Y), azar(150, 400));
    esperar(azar(400, 1500));
}

static void juego_centro(void)
{
    animar(CARA_NORMAL, 300);
    esperar(azar(300, 1000));
}

static void juego_doble_parpadeo(void)
{
    cerrar_y_abrir(true, true, 30);
    esperar(80);
    cerrar_y_abrir(true, true, 30);
}

static void juego_guino(void)
{
    cara_t d = cara;
    d.feliz = 0.5f;
    animar(d, 150);
    bool izquierdo = azar(0, 1);
    cerrar_y_abrir(izquierdo, !izquierdo, 350);
    d = cara;
    d.feliz = 0;
    animar(d, 150);
}

static void juego_pasear(void)
{
    int lado = azar(0, 1) ? 1 : -1;
    mover_a(lado * MAX_X, azar(-MAX_Y, MAX_Y), 400);
    esperar(300);
    mover_a(-lado * MAX_X, azar(-MAX_Y, MAX_Y), 700);
    esperar(300);
    mover_a(0, 0, 400);
}

static void juego_contenta(void)
{
    cara_contenta(NULL, 400);
    cara_tranquila();
}

typedef struct { const char *nombre; void (*hacer)(void); int peso; } juego_t;

static const juego_t juegos[] = {
    { "mirar",          juego_mirar,          10 },
    { "centro",         juego_centro,          4 },
    { "doble parpadeo", juego_doble_parpadeo,  3 },
    { "guino",          juego_guino,           2 },
    { "pasear",         juego_pasear,          2 },
    { "contenta",       juego_contenta,        2 },
};
#define CANT_JUEGOS ((int)(sizeof(juegos) / sizeof(juegos[0])))

// ─── Funciones publicas ──────────────────────────────────────────────────────

void cara_iniciar(const cara_colores_t *colores)
{
    col = *colores;
    cara = CARA_NORMAL;
    texto_abajo = NULL;
    dibujar();
}

void cara_parpadear(void)
{
    cortado = false;
    cerrar_y_abrir(true, true, 40);
}

void cara_jugar(void)
{
    cortado = false;
    int total = 0;
    for (int i = 0; i < CANT_JUEGOS; i++) total += juegos[i].peso;
    int r = azar(0, total - 1);
    for (int i = 0; i < CANT_JUEGOS; i++) {
        if (r < juegos[i].peso) {
            ESP_LOGI(TAG, "Juego: %s", juegos[i].nombre);
            juegos[i].hacer();
            return;
        }
        r -= juegos[i].peso;
    }
}

void cara_mirar_hacia(int x, int y, int ms)
{
    cortado = false;
    float dx = (x - LCD_H_RES / 2.0f) / (LCD_H_RES / 2.0f);          // -1 (izquierda) a 1 (derecha)
    float dy = (y - (float)OJOS_Y) / (LCD_V_RES / 2.0f);             // -1 (arriba) a 1 (abajo)
    if (dx < -1) dx = -1;
    if (dx >  1) dx =  1;
    if (dy < -1) dy = -1;
    if (dy >  1) dy =  1;
    cara_t d = cara;
    d.x = dx * MAX_X;
    d.y = dy * MAX_Y;
    animar(d, ms);
}

void cara_contenta(const char *texto, int ms)
{
    cortado = false;
    texto_abajo = texto;
    cara_t d = CARA_NORMAL;
    d.feliz = 1;
    animar(d, 200);
    for (int i = 0; i < 3; i++) {                     // saltitos
        mover_a(0, -14, 120);
        mover_a(0, 0, 120);
    }
    esperar(ms);
    texto_abajo = NULL;
}

void cara_set_voz(float (*nivel)(void))
{
    nivel_voz = nivel;
}

void cara_mientras_suena(bool (*sonando)(void))
{
    TickType_t ultimo = xTaskGetTickCount();
    while (sonando()) {
        dibujar();
        if (xTaskDelayUntil(&ultimo, pdMS_TO_TICKS(CUADRO_MS)) == pdFALSE) {
            vTaskDelay(1);
            ultimo = xTaskGetTickCount();
        }
    }
    boca_abierta = 0;
    dibujar();
}

void cara_set_oido(float (*nivel)(void))
{
    nivel_oido = nivel;
}

void cara_escuchar(const char *texto)
{
    cortado = false;
    texto_abajo = texto;
    cara_t d = CARA_NORMAL;
    d.escucha = 1;
    d.y = -12;                       // mira un poquito para arriba, atenta
    animar(d, 200);
}

void cara_un_cuadro(void)
{
    dibujar();
    vTaskDelay(pdMS_TO_TICKS(CUADRO_MS));
}

void cara_dejar_de_escuchar(void)
{
    cortado = false;
    texto_abajo = NULL;
    animar(CARA_NORMAL, 200);
}

void cara_poner_texto(const char *texto)
{
    texto_abajo = texto;
}

void cara_pensar(void)
{
    cortado = false;
    cara_t d = CARA_NORMAL;
    d.x = MAX_X * 0.6f;                  // mira para arriba y a un costado, como pensando
    d.y = -MAX_Y;
    animar(d, 300);
}

void cara_tranquila(void)
{
    cortado = false;
    texto_abajo = NULL;
    animar(CARA_NORMAL, 250);
}
