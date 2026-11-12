#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

#include "trumi_config.h"
#include "board_config.h"
#include "placa.h"
#include "display.h"
#include "tactil.h"
#include "cara.h"
#include "audio.h"
#include "voz.h"
#include "wifi.h"
#include "cerebro.h"

static const char *TAG = "TRUMI";

// ─── Etapas 1 a 5: la cara, el tactil, la voz, los oidos y la charla ─────────
// Trumi juega con sus ojos. Cuando la tocan, hace "bip", sus ojos siguen al dedo
// y cuando lo sueltan se pone contenta y dice algo (¡con su voz!).
// Si dejan el dedo apoyado, se pone a escuchar: graba lo que le dicen mientras el
// dedo sigue apoyado y, al soltarlo, se lo manda a su cerebro (en la Raspberry),
// pone cara de pensar y CONTESTA. Si al prenderse no tenia red o no encontro al
// cerebro (o el cerebro falla), anda sola como antes: juega al lorito, repite lo
// que escucho.
// Y si escucha un ruido fuerte mientras esta tranquila, pregunta "¿Me hablaste?".

static int16_t *grabacion = NULL;            // lo que escucha el lorito (en la PSRAM)
static size_t   max_grabacion = 0;
static int64_t  ultimo_ruido_us = 0;         // cuando reacciono a un ruido por ultima vez
static int64_t  ultimo_sonido_us = 0;        // cuando hablo ella por ultima vez (para no escucharse)

static int64_t ahora_us(void) { return esp_timer_get_time(); }

static char texto_respuesta[CEREBRO_MAX_TEXTO + 32];   // la respuesta, partida en renglones

// Parte el texto en renglones de hasta "letras" letras, cortando en los espacios
// (¡ y ¿ ocupan 2 bytes, pero son una letra)
static void partir_en_renglones(const char *texto, char *salida, size_t tamano, int letras)
{
    size_t n = 0;
    const char *p = texto;
    while (*p && n + 2 < tamano) {
        while (*p == ' ') p++;                         // sin espacios al principio del renglon
        const char *fin = p, *ultimo_espacio = NULL;
        int cuenta = 0;
        while (*fin) {
            if (((unsigned char)*fin & 0xC0) != 0x80) {
                if (cuenta == letras) break;
                cuenta++;
            }
            if (*fin == ' ') ultimo_espacio = fin;
            fin++;
        }
        const char *corte = (*fin && ultimo_espacio != NULL) ? ultimo_espacio : fin;
        if (n > 0 && corte > p) salida[n++] = '\n';
        while (p < corte && n + 1 < tamano) salida[n++] = *p++;
    }
    salida[n] = '\0';
}

// ─── Sonidos ─────────────────────────────────────────────────────────────────
// Notas: DO = 523 Hz, MI = 659, SOL = 784, DO agudo = 1047...
static const audio_nota_t sonido_arranque[] = {
    { 523, 90 }, { 659, 90 }, { 784, 90 }, { 1047, 220 },
};
static const audio_nota_t sonido_toque[] = {
    { 880, 40 }, { 1320, 60 },
};
#define CANT(x) ((int)(sizeof(x) / sizeof((x)[0])))

// Para que la cara corte lo que esta haciendo apenas la tocan
static bool me_tocaron(void)
{
    return tactil_hay_toque_nuevo();
}

// Lo que dice cuando la tocan (en la pantalla y con su voz)
static const struct { const char *texto; voz_t voz; } frases[] = {
    { "¡Hola!",                 VOZ_HOLA },
    { "¡Jiji!",                 VOZ_JIJI },
    { "¡Me hiciste\ncosquillas!", VOZ_COSQUILLAS },
    { "¡Te quiero!",            VOZ_TE_QUIERO },
    { "¡Otra vez!",             VOZ_OTRA_VEZ },
};

// Habla y espera a terminar de hablar (con la boca moviendose)
static void terminar_de_hablar(void)
{
    cara_mientras_suena(audio_sonando);
    ultimo_sonido_us = ahora_us();
}

// Le manda al cerebro lo que escucho, pone cara de pensar y contesta con su voz.
// Devuelve false si no se puede charlar (sin red, o el cerebro no esta o fallo):
// en ese caso, Trumi hace de lorito.
static bool contestar_con_el_cerebro(size_t muestras)
{
    if (!cerebro_disponible()) return false;
    if (!cerebro_preguntar(grabacion, muestras)) return false;

    ESP_LOGI(TAG, "Pensando...");
    cara_pensar();
    static const char *const pensando[] = { "Mmm", "Mmm.", "Mmm..", "Mmm..." };
    for (int i = 0; !cerebro_listo(); i++) {
        cara_poner_texto(pensando[(i / 12) % 4]);       // los puntitos van apareciendo
        tactil_se_toco(NULL, NULL);                    // mientras piensa, los toques no cuentan
        cara_un_cuadro();
    }
    cara_poner_texto(NULL);

    const cerebro_respuesta_t *r = cerebro_respuesta();
    if (!r->ok || r->muestras == 0) {
        ESP_LOGW(TAG, "El cerebro no contesto (%s): hago de lorito", r->error);
        cara_tranquila();
        return false;                                  // repite lo que escucho (lorito)
    }

    // ¡Contesta! Con la voz que le mando el cerebro y el texto en la pantalla
    partir_en_renglones(r->pantalla, texto_respuesta, sizeof(texto_respuesta), 24);
    cara_tranquila();
    audio_reproducir(r->audio, r->muestras * sizeof(int16_t));
    cara_poner_texto(texto_respuesta);
    terminar_de_hablar();
    for (int i = 0; i < MOSTRAR_RESPUESTA_MS / 30 && !tactil_hay_toque_nuevo(); i++)
        cara_un_cuadro();                              // que se pueda terminar de leer
    cara_poner_texto(NULL);
    cara_tranquila();
    return true;
}

// Escucha mientras el dedo siga apoyado. Despues le pregunta al cerebro y contesta,
// o (si no puede charlar) juega al lorito: repite lo que escucho.
static void escuchar(void)
{
    ESP_LOGI(TAG, "Escuchando...");
    audio_grabar_empezar(grabacion, max_grabacion);
    cara_escuchar("Te escucho...");
    while (tactil_tocando(NULL, NULL) && !audio_grabar_lleno()) {
        tactil_se_toco(NULL, NULL);
        cara_un_cuadro();
    }
    size_t muestras = audio_grabar_terminar();
    tactil_se_toco(NULL, NULL);
    cara_dejar_de_escuchar();
    ESP_LOGI(TAG, "Escuche %d ms", (int)(muestras * 1000 / AUDIO_MUESTRAS_POR_SEGUNDO));

    if (muestras < AUDIO_MUESTRAS_POR_SEGUNDO / 3) {  // menos de un tercio de segundo: no escucho nada
        cara_contenta("¡No te\nescuche!", 600);
        cara_tranquila();
        return;
    }
    if (contestar_con_el_cerebro(muestras)) return;    // ¡charla!

    audio_reproducir(grabacion, muestras * sizeof(int16_t));   // el lorito: ¡repite lo que escucho!
    cara_contenta("¡Escucha!", 200);
    terminar_de_hablar();
    cara_tranquila();
}

// Cuando la tocan: "bip", mientras el dedo esta apoyado los ojos lo siguen, y
// cuando lo sueltan se pone contenta y dice algo. Si dejan el dedo apoyado un
// ratito, se pone a escuchar (y despues contesta, o hace de lorito).
static void atender_toque(void)
{
    int x, y;
    tactil_se_toco(&x, &y);                           // este toque ya lo estamos atendiendo
    ESP_LOGI(TAG, "¡Me tocaron! (%d, %d)", x, y);
    audio_melodia(sonido_toque, CANT(sonido_toque));
    int64_t inicio = ahora_us();
    cara_mirar_hacia(x, y, 120);
    while (tactil_tocando(&x, &y)) {
        tactil_se_toco(NULL, NULL);                    // si levantan y vuelven a apoyar, es el mismo juego
        if (audio_mic_listo() && grabacion != NULL &&
            ahora_us() - inicio > (int64_t)MANTENER_PARA_ESCUCHAR_MS * 1000) {
            escuchar();                                // dejo el dedo apoyado: ¡a escuchar!
            return;
        }
        cara_mirar_hacia(x, y, 60);
    }
    tactil_se_toco(NULL, NULL);

    int i = esp_random() % CANT(frases);
    voz_decir(frases[i].voz);                          // empieza a hablar...
    cara_contenta(frases[i].texto, 300);               // ...mientras se pone contenta
    terminar_de_hablar();                              // y termina de hablar
    cara_tranquila();
}

// ¿Escucho un ruido fuerte? (no cuenta si esta hablando ella, ni recien despues)
static bool escucho_un_ruido(void)
{
    if (!audio_mic_listo() || audio_sonando()) return false;
    int64_t ahora = ahora_us();
    if (ahora - ultimo_sonido_us < 800 * 1000) return false;
    if (ultimo_ruido_us != 0 && ahora - ultimo_ruido_us < (int64_t)ESPERA_ENTRE_RUIDOS_MS * 1000) return false;
    return audio_mic_nivel() > UMBRAL_RUIDO;
}

// Escucho un ruido: pone cara de escuchar un ratito y pregunta
static void reaccionar_al_ruido(void)
{
    ultimo_ruido_us = ahora_us();
    ESP_LOGI(TAG, "Escuche un ruido (nivel %.2f)", audio_mic_nivel());
    cara_escuchar("¿Me\nhablaste?");
    for (int i = 0; i < 1500 / 30 && !tactil_hay_toque_nuevo(); i++) cara_un_cuadro();
    cara_dejar_de_escuchar();
}

// Un descansito, pero corta si la tocan o si escucha un ruido
static void descansar(int ms)
{
    for (int t = 0; t < ms && !tactil_hay_toque_nuevo(); t += 20) {
        if (escucho_un_ruido()) {
            reaccionar_al_ruido();
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "¡Hola! Soy Trumi :)");

    placa_iniciar();                   // bus I2C, expansor (luz de la pantalla, reset, amplificador)
    display_init();
    tactil_iniciar();
    audio_iniciar();
    audio_volumen(VOLUMEN);
    wifi_iniciar();                    // se conecta "de fondo"
    cerebro_iniciar();

    // Lugar para lo que escucha el lorito (en la PSRAM: 16.000 muestras por segundo)
    max_grabacion = (size_t)GRABAR_MAX_S * AUDIO_MUESTRAS_POR_SEGUNDO;
    grabacion = heap_caps_malloc(max_grabacion * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (grabacion == NULL) ESP_LOGE(TAG, "No hay memoria para grabar");

    // Saludo inicial (la luz se prende cuando ya hay algo dibujado)
    display_clear(COLOR_FONDO);
    display_draw_text_centered(MENSAJE_ARRANQUE, -1, MENSAJE_ARRANQUE_TAMANO, COLOR_TEXTO);
    display_flush();
    placa_brillo(BRILLO_PANTALLA);
    audio_melodia(sonido_arranque, CANT(sonido_arranque));
    voz_decir(VOZ_HOLA_SOY_TRUMI);
    vTaskDelay(pdMS_TO_TICKS(MENSAJE_ARRANQUE_MS));
    audio_esperar();

    const cara_colores_t colores = {
        .fondo = COLOR_FONDO, .ojos = COLOR_OJOS, .brillo = COLOR_BRILLO,
        .pestanas = COLOR_PESTANAS, .cachetes = COLOR_CACHETES,
        .boca = COLOR_BOCA, .boca_adentro = COLOR_BOCA_ADENTRO,
    };
    cara_set_interrupcion(me_tocaron);
    cara_set_voz(audio_nivel);         // la boca se mueve con lo que suena
    cara_set_oido(audio_mic_nivel);    // las ondas se mueven con lo que escucha
    cara_iniciar(&colores);
    voz_decir(VOZ_HOLA_SOFI);
    cara_contenta(SALUDO, 300);
    terminar_de_hablar();
    cara_tranquila();

    bool ya_festejo_wifi = false;
    while (1) {
        if (tactil_hay_toque_nuevo()) {
            atender_toque();
            continue;
        }

        // La primera vez que se conecta al WiFi, lo festeja
        if (!ya_festejo_wifi && wifi_conectado()) {
            ya_festejo_wifi = true;
            ESP_LOGI(TAG, "Tengo wifi!");
            cara_contenta("¡Tengo wifi!", 600);
            cara_tranquila();
        }

        // Juega con sus ojos
        cara_jugar();

        // Un descansito entre juego y juego, y a veces un parpadeo
        descansar(DESCANSO_MIN_MS + esp_random() % (DESCANSO_MAX_MS - DESCANSO_MIN_MS));
        if (!tactil_hay_toque_nuevo() && esp_random() % 2) cara_parpadear();
    }
}
