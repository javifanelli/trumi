#include "cerebro.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "wifi.h"

// La direccion y la clave estan en cerebro_secretos.h, que NO se sube al repo (es
// publico). Si el archivo existe, CMakeLists.txt define TRUMI_HAY_SECRETOS_CEREBRO.
#ifdef TRUMI_HAY_SECRETOS_CEREBRO
#include "cerebro_secretos.h"
#endif

static const char *TAG = "CEREBRO";

/*
 * Como le habla Trumi al cerebro (ver cerebro/README.md):
 *   POST CEREBRO_URL/charlar   con el encabezado X-Clave y, en el cuerpo, el audio
 *                              crudo de lo que escucho (16 bits, mono, 16 kHz)
 *   Contesta con el audio de la respuesta (mismo formato) y estos encabezados:
 *     X-Escuche  = lo que entendio
 *     X-Pantalla = lo que contesta, sin tildes
 *   (el texto viene "percent-encoded": %C2%A1 = ¡ ...)
 *
 * La charla tarda varios segundos (escuchar, pensar y hablar), asi que se hace
 * en una tarea aparte: mientras tanto, Trumi pone cara de pensar.
 *
 * Al prenderse, Trumi espera el WiFi un ratito y le pregunta al cerebro si esta
 * (GET /estado). Si no hay red o el cerebro no contesta, se queda sin cerebro
 * hasta que la apaguen (y juega al lorito). Si en una charla el cerebro falla,
 * tambien se queda sin cerebro.
 */
#define MAX_RESPUESTA_S  30                    // la respuesta hablada mas larga que entra
#define ESPERA_MAX_MS    (150 * 1000)          // si el cerebro no contesta en este tiempo, se corta
#define ESPERA_WIFI_S    20                    // al prenderse, cuanto espera el WiFi antes de rendirse
#define ESPERA_ESTADO_MS 4000                  // cuanto espera que el cerebro conteste "estoy"

static int16_t *lugar_audio = NULL;            // donde se guarda la respuesta hablada (PSRAM)
static size_t   max_muestras = 0;
static cerebro_respuesta_t respuesta;
static volatile bool pensando = false;
static volatile bool disponible = false;      // true si el cerebro contesto al prenderse (y no fallo despues)

#ifdef TRUMI_HAY_SECRETOS_CEREBRO

static const int16_t *pregunta_audio;   // lo que escucho (mientras se lo manda)
static size_t pregunta_muestras;

// "%C2%A1Hola" -> "¡Hola" (y "+" -> espacio)
static void decodificar(const char *de, char *a, size_t tamano)
{
    size_t n = 0;
    while (*de && n + 1 < tamano) {
        if (de[0] == '%' && isxdigit((unsigned char)de[1]) && isxdigit((unsigned char)de[2])) {
            char hex[3] = { de[1], de[2], 0 };
            a[n++] = (char)strtol(hex, NULL, 16);
            de += 3;
        } else {
            a[n++] = (*de == '+') ? ' ' : *de;
            de++;
        }
    }
    a[n] = '\0';
}

static esp_err_t al_recibir(esp_http_client_event_t *e)
{
    if (e->event_id == HTTP_EVENT_ON_HEADER) {
        if (strcasecmp(e->header_key, "X-Escuche") == 0)
            decodificar(e->header_value, respuesta.escuche, sizeof(respuesta.escuche));
        else if (strcasecmp(e->header_key, "X-Pantalla") == 0)
            decodificar(e->header_value, respuesta.pantalla, sizeof(respuesta.pantalla));
    }
    return ESP_OK;
}

static void fallar(const char *motivo)
{
    snprintf(respuesta.error, sizeof(respuesta.error), "%s", motivo);
    respuesta.ok = false;
    disponible = false;                        // desde ahora, sin cerebro: juega al lorito
    ESP_LOGW(TAG, "%s. Sigo sin cerebro (lorito)", motivo);
}

static void charlar(void)
{
    esp_http_client_config_t cfg = {
        .url            = CEREBRO_URL "/charlar",
        .method         = HTTP_METHOD_POST,
        .timeout_ms     = ESPERA_MAX_MS,
        .event_handler  = al_recibir,
        .buffer_size    = 4096,                // los encabezados con texto pueden ser largos
        .buffer_size_tx = 1024,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (c == NULL) { fallar("No pude preparar la conexion"); return; }
    esp_http_client_set_header(c, "Content-Type", "application/octet-stream");
    esp_http_client_set_header(c, "X-Clave", CEREBRO_CLAVE);

    int bytes = (int)(pregunta_muestras * sizeof(int16_t));
    if (esp_http_client_open(c, bytes) != ESP_OK) {
        ESP_LOGW(TAG, "No pude conectarme a %s", CEREBRO_URL);
        fallar("No encuentro al cerebro (¿esta prendida la Raspberry?)");
        esp_http_client_cleanup(c);
        return;
    }
    // Mandar lo que escucho (de a pedazos)
    const char *p = (const char *)pregunta_audio;
    for (int enviados = 0; enviados < bytes; ) {
        int n = esp_http_client_write(c, p + enviados, bytes - enviados);
        if (n <= 0) { fallar("Se corto mientras mandaba el audio"); goto fin; }
        enviados += n;
    }
    // Esperar la respuesta (aca tarda: escucha, piensa y habla)
    esp_http_client_fetch_headers(c);
    int codigo = esp_http_client_get_status_code(c);
    if (codigo == 403) { fallar("La clave no coincide con la del cerebro"); goto fin; }
    if (codigo != 200) {
        ESP_LOGW(TAG, "Le pedi a %s/charlar (¿es la direccion y el puerto del cerebro?)", CEREBRO_URL);
        char motivo[48];
        snprintf(motivo, sizeof(motivo), "El cerebro contesto con el codigo %d", codigo);
        fallar(motivo);
        goto fin;
    }
    // Leer la respuesta hablada
    size_t max_bytes = max_muestras * sizeof(int16_t), leidos = 0;
    char *destino = (char *)lugar_audio;
    while (leidos < max_bytes) {
        int n = esp_http_client_read(c, destino + leidos, (int)(max_bytes - leidos));
        if (n <= 0) break;
        leidos += n;
    }
    respuesta.audio = lugar_audio;
    respuesta.muestras = leidos / sizeof(int16_t);
    respuesta.ok = true;
    ESP_LOGI(TAG, "Entendio \"%s\" y contesta \"%s\" (%d ms de voz)", respuesta.escuche, respuesta.pantalla,
             (int)(respuesta.muestras * 1000 / 16000));
fin:
    esp_http_client_close(c);
    esp_http_client_cleanup(c);
}

static void tarea_charla(void *arg)
{
    charlar();
    pensando = false;
    vTaskDelete(NULL);
}

// Al prenderse: espera el WiFi y le pregunta al cerebro si esta
static void tarea_comprobar(void *arg)
{
    for (int i = 0; i < ESPERA_WIFI_S * 10 && !wifi_conectado(); i++) vTaskDelay(pdMS_TO_TICKS(100));
    if (!wifi_conectado()) {
        ESP_LOGW(TAG, "No hay WiFi: Trumi anda sola (lorito)");
        vTaskDelete(NULL);
        return;
    }
    esp_http_client_config_t cfg = {
        .url        = CEREBRO_URL "/estado",
        .method     = HTTP_METHOD_GET,
        .timeout_ms = ESPERA_ESTADO_MS,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    esp_err_t err = (c != NULL) ? esp_http_client_perform(c) : ESP_FAIL;
    int codigo = (c != NULL) ? esp_http_client_get_status_code(c) : 0;
    if (c != NULL) esp_http_client_cleanup(c);

    disponible = (err == ESP_OK && codigo == 200);
    if (disponible) ESP_LOGI(TAG, "¡El cerebro esta! (%s): Trumi puede charlar", CEREBRO_URL);
    else            ESP_LOGW(TAG, "El cerebro no contesta en %s (codigo %d): Trumi anda sola (lorito)",
                             CEREBRO_URL, codigo);
    vTaskDelete(NULL);
}

#endif // TRUMI_HAY_SECRETOS_CEREBRO

// ─── Publico ─────────────────────────────────────────────────────────────────

void cerebro_iniciar(void)
{
    max_muestras = (size_t)MAX_RESPUESTA_S * 16000;
    lugar_audio = heap_caps_malloc(max_muestras * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (lugar_audio == NULL) ESP_LOGE(TAG, "No hay memoria para las respuestas");
#ifdef TRUMI_HAY_SECRETOS_CEREBRO
    if (lugar_audio != NULL) xTaskCreate(tarea_comprobar, "comprobar", 4096, NULL, 4, NULL);
#else
    ESP_LOGW(TAG, "No hay datos del cerebro: Trumi juega al lorito. Para charlar, copia "
                  "cerebro_secretos.ejemplo.h como cerebro_secretos.h y completalo");
#endif
}

bool cerebro_configurado(void)
{
#ifdef TRUMI_HAY_SECRETOS_CEREBRO
    return lugar_audio != NULL;
#else
    return false;
#endif
}

bool cerebro_disponible(void)
{
    return disponible && wifi_conectado();
}

bool cerebro_preguntar(const int16_t *audio, size_t muestras)
{
#ifdef TRUMI_HAY_SECRETOS_CEREBRO
    if (!cerebro_disponible() || pensando) return false;
    memset(&respuesta, 0, sizeof(respuesta));
    pregunta_audio = audio;
    pregunta_muestras = muestras;
    pensando = true;
    if (xTaskCreate(tarea_charla, "charla", 6144, NULL, 5, NULL) != pdPASS) {
        pensando = false;
        return false;
    }
    return true;
#else
    (void)audio; (void)muestras;
    return false;
#endif
}

bool cerebro_listo(void)
{
    return !pensando;
}

const cerebro_respuesta_t *cerebro_respuesta(void)
{
    return &respuesta;
}
