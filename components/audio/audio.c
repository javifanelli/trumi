#include "audio.h"
#include "placa.h"
#include "board_config.h"

#include <math.h>
#include <string.h>
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "AUDIO";

/*
 * Como suena Trumi:
 *   ESP32  ──I2S──►  ES8311 (convierte los numeros en "electricidad de sonido")
 *                      ──►  HT6872 (amplificador)  ──►  parlante
 *
 *   - El I2S es un bus para audio: un cable marca el ritmo (BCLK), otro dice si
 *     va el lado izquierdo o el derecho (LRCK), y por otro van los numeros.
 *     Ademas el MCLK es un "reloj maestro" que el ES8311 necesita para funcionar.
 *   - Al ES8311 hay que configurarlo antes por I2C (sus "registros"): que tome
 *     el reloj del MCLK, que espere numeros de 16 bits, prender sus partes y el
 *     volumen. La secuencia sale del driver de Espressif para este chip.
 *   - El amplificador se prende solo mientras suena algo (asi no hace "shhh").
 *
 * Como escucha Trumi:
 *   2 microfonos  ──►  ES7210 (convierte el sonido en numeros)  ──I2S──►  ESP32
 *
 *   - Usa el mismo bus I2S que el parlante, pero por otro cable de datos (DIN):
 *     el bus va en las dos direcciones a la vez ("full duplex").
 *   - Cada microfono va por un lado del canal (izquierdo y derecho). Trumi los
 *     junta en uno solo (los suma, y despues los refuerza un poco mas).
 *   - Una tarea lee los microfonos todo el tiempo: mide que tan fuerte se
 *     escucha y, si le pidieron, guarda lo que escucha (graba).
 */
#define ES8311_DIRECCION_1 0x18     // segun como este conectado su pin CE: 0x18 o 0x19
#define ES8311_DIRECCION_2 0x19
#define REG_VOLUMEN        0x32     // volumen del DAC: 0x00 = mudo ... 0xBF = 0 dB
#define MIC_GANANCIA       14       // ganancia de los microfonos en el ES7210: 0 = 0 dB, cada +1 = +3 dB (14 = 37,5 dB, el maximo)
#define MIC_REFUERZO       2        // y despues se multiplica por esto (refuerzo por programa)
#define MUESTRAS_POR_VUELTA 256     // de a cuantas muestras se manda el audio
#define PAUSA_APAGAR_MS    200      // cuanto espera en silencio antes de apagar el amplificador

static i2s_chan_handle_t salida, entrada;
static i2c_master_dev_handle_t es8311, es7210;
static QueueHandle_t fila;
static volatile int pendientes = 0;     // cuantas cosas faltan sonar (en la fila o sonando)
static volatile float nivel = 0;

// Microfonos
static volatile float nivel_mic = 0;
static volatile bool grabando = false;
static int16_t *grabacion = NULL;
static volatile size_t grabadas = 0, max_grabar = 0;

// Lo que puede haber en la fila: un audio guardado o una nota
typedef struct {
    const uint8_t *pcm;    // NULL = es una nota
    size_t bytes;
    int hz, ms;
} trabajo_t;

// ─── El ES8311 ───────────────────────────────────────────────────────────────

static esp_err_t escribir_en(i2c_master_dev_handle_t chip, uint8_t registro, uint8_t valor)
{
    uint8_t buf[2] = { registro, valor };
    return i2c_master_transmit(chip, buf, 2, 100);
}

static esp_err_t escribir(uint8_t registro, uint8_t valor)
{
    return escribir_en(es8311, registro, valor);
}

static bool preparar_es8311(void)
{
    i2c_master_bus_handle_t bus = placa_bus_i2c();
    uint16_t direccion = 0;
    if (i2c_master_probe(bus, ES8311_DIRECCION_1, 100) == ESP_OK)      direccion = ES8311_DIRECCION_1;
    else if (i2c_master_probe(bus, ES8311_DIRECCION_2, 100) == ESP_OK) direccion = ES8311_DIRECCION_2;
    else {
        ESP_LOGE(TAG, "El ES8311 no contesta (ni en 0x18 ni en 0x19)");
        return false;
    }
    i2c_device_config_t dev = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = direccion,
        .scl_speed_hz    = I2C_HZ,
    };
    if (i2c_master_bus_add_device(bus, &dev, &es8311) != ESP_OK) return false;
    ESP_LOGI(TAG, "ES8311 encontrado en 0x%02X", direccion);

    // { registro, valor }: la secuencia de arranque (del driver de Espressif)
    static const uint8_t pasos[][2] = {
        { 0x45, 0x00 },
        { 0x01, 0x30 }, { 0x02, 0x10 }, { 0x02, 0x00 }, { 0x03, 0x10 }, { 0x16, 0x24 },
        { 0x04, 0x10 }, { 0x05, 0x00 }, { 0x0B, 0x00 }, { 0x0C, 0x00 }, { 0x10, 0x1F },
        { 0x11, 0x7F },
        { 0x00, 0x80 },     // encendido, modo "esclavo" (el ritmo lo marca el ESP32)
        { 0x01, 0x3F },     // reloj desde el pin MCLK, todos los relojes prendidos
        // Divisores para MCLK = 256 x 16 kHz (4,096 MHz)
        { 0x02, 0x00 }, { 0x05, 0x00 }, { 0x03, 0x10 }, { 0x04, 0x10 },
        { 0x07, 0x00 }, { 0x08, 0xFF }, { 0x06, 0x03 },
        { 0x09, 0x0C },     // entrada de audio: I2S, 16 bits
        { 0x0A, 0x0C },     // salida de audio (microfono): I2S, 16 bits
        { 0x0D, 0x01 },     // prender la parte analogica
        { 0x0E, 0x02 },
        { 0x12, 0x00 },     // prender el DAC (el que hace sonar)
        { 0x13, 0x10 },     // salida al amplificador
        { 0x1C, 0x6A },
        { 0x37, 0x08 },     // sin ecualizador
    };
    for (unsigned i = 0; i < sizeof(pasos) / sizeof(pasos[0]); i++) {
        if (escribir(pasos[i][0], pasos[i][1]) != ESP_OK) {
            ESP_LOGE(TAG, "No pude configurar el ES8311 (registro 0x%02X)", pasos[i][0]);
            return false;
        }
    }
    return true;
}

// ─── El ES7210 (microfonos) ──────────────────────────────────────────────────

static esp_err_t leer_es7210(uint8_t registro, uint8_t *valor)
{
    return i2c_master_transmit_receive(es7210, &registro, 1, valor, 1, 100);
}

static bool preparar_es7210(void)
{
    i2c_master_bus_handle_t bus = placa_bus_i2c();
    uint16_t direccion = 0;
    for (uint16_t d = 0x40; d <= 0x43 && direccion == 0; d++)       // segun sus pines AD0 y AD1
        if (i2c_master_probe(bus, d, 100) == ESP_OK) direccion = d;
    if (direccion == 0) {
        ESP_LOGE(TAG, "El ES7210 (microfonos) no contesta (0x40 a 0x43)");
        return false;
    }
    i2c_device_config_t dev = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = direccion,
        .scl_speed_hz    = I2C_HZ,
    };
    if (i2c_master_bus_add_device(bus, &dev, &es7210) != ESP_OK) return false;
    uint8_t id1 = 0, id2 = 0;
    leer_es7210(0x3D, &id1);
    leer_es7210(0x3E, &id2);
    ESP_LOGI(TAG, "ES7210 encontrado en 0x%02X (id %02X%02X)", direccion, id1, id2);

    // { registro, valor }: la secuencia de arranque (del driver de Espressif),
    // modo "esclavo", I2S normal de 16 bits, microfonos 1 y 2, a 16 kHz con MCLK = 256 x fs
    static const uint8_t pasos[][2] = {
        { 0x00, 0xFF }, { 0x00, 0x41 },             // reset
        { 0x01, 0x1F },                             // relojes apagados mientras se configura
        { 0x09, 0x30 }, { 0x0A, 0x30 },             // tiempos de encendido
        { 0x23, 0x2A }, { 0x22, 0x0A }, { 0x20, 0x0A }, { 0x21, 0x2A },   // filtros
        { 0x08, 0x00 },                             // modo esclavo (el ritmo lo marca el ESP32): ver abajo
        { 0x40, 0x43 },                             // parte analogica, 3,3 V
        { 0x41, 0x70 }, { 0x42, 0x70 },             // tension para los microfonos (2,87 V)
        { 0x02, 0x81 },                             // divisor del reloj (para 16 kHz)
        { 0x07, 0x20 },                             // sobremuestreo
        { 0x04, 0x01 }, { 0x05, 0x00 },             // divisor del LRCK (256)
        { 0x11, 0x60 },                             // formato: I2S, 16 bits
        { 0x12, 0x00 },                             // sin TDM: mic 1 a la izquierda, mic 2 a la derecha
        { 0x43, 0x10 | MIC_GANANCIA }, { 0x44, 0x10 | MIC_GANANCIA },   // ganancia de los mics 1 y 2
        { 0x45, 0x00 }, { 0x46, 0x00 },             // mics 3 y 4 sin usar
        { 0x4B, 0x00 },                             // prender los mics 1 y 2
        { 0x4C, 0xFF },                             // apagar los mics 3 y 4
        { 0x01, 0x14 },                             // prender los relojes de los mics 1 y 2
        { 0x06, 0x00 },                             // encender
        { 0x47, 0x08 }, { 0x48, 0x08 }, { 0x49, 0x08 }, { 0x4A, 0x08 },
    };
    for (unsigned i = 0; i < sizeof(pasos) / sizeof(pasos[0]); i++) {
        uint8_t valor = pasos[i][1];
        if (pasos[i][0] == 0x08) {                  // en el 0x08 solo se apaga el bit 0 (esclavo)
            uint8_t actual = 0;
            leer_es7210(0x08, &actual);
            valor = actual & 0xFE;
        }
        if (escribir_en(es7210, pasos[i][0], valor) != ESP_OK) {
            ESP_LOGE(TAG, "No pude configurar el ES7210 (registro 0x%02X)", pasos[i][0]);
            return false;
        }
    }
    return true;
}

// ─── El I2S ──────────────────────────────────────────────────────────────────

static void preparar_i2s(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;            // si no hay nada para mandar, manda silencio
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &salida, &entrada));   // las dos direcciones

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_MUESTRAS_POR_SEGUNDO),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = PIN_I2S_MCLK,
            .bclk = PIN_I2S_BCLK,
            .ws   = PIN_I2S_LRCK,
            .dout = PIN_I2S_DOUT,
            .din  = PIN_I2S_DIN,
        },
    };
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(salida, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(entrada, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(salida));
    ESP_ERROR_CHECK(i2s_channel_enable(entrada));
}

// ─── La tarea que escucha ────────────────────────────────────────────────────

static void tarea_microfono(void *arg)
{
    static int16_t estereo[MUESTRAS_POR_VUELTA * 2];
    while (1) {
        size_t leidos = 0;
        if (i2s_channel_read(entrada, estereo, sizeof(estereo), &leidos, portMAX_DELAY) != ESP_OK) continue;
        int n = (int)(leidos / (2 * sizeof(int16_t)));
        float suma = 0;
        for (int i = 0; i < n; i++) {
            // Suma de los 2 mics, reforzada, sin pasarse de lo que entra en 16 bits
            int32_t suma_mics = ((int32_t)estereo[2 * i] + estereo[2 * i + 1]) * MIC_REFUERZO;
            if (suma_mics >  32767) suma_mics =  32767;
            if (suma_mics < -32768) suma_mics = -32768;
            int16_t mono = (int16_t)suma_mics;
            suma += (float)mono * mono;
            if (grabando && grabadas < max_grabar) grabacion[grabadas++] = mono;
        }
        float fuerza = sqrtf(suma / (n > 0 ? n : 1)) / 6000.0f;
        nivel_mic = fuerza > 1 ? 1 : fuerza;
    }
}

// ─── La tarea que hace sonar ─────────────────────────────────────────────────

// Manda un pedacito de audio mono (lo copia a los dos lados) y mide que tan fuerte es
static void mandar(const int16_t *mono, int n)
{
    static int16_t estereo[MUESTRAS_POR_VUELTA * 2];
    float suma = 0;
    for (int i = 0; i < n; i++) {
        estereo[2 * i] = estereo[2 * i + 1] = mono[i];
        suma += (float)mono[i] * mono[i];
    }
    float fuerza = sqrtf(suma / (n > 0 ? n : 1)) / 8000.0f;
    nivel = fuerza > 1 ? 1 : fuerza;
    size_t escritos;
    i2s_channel_write(salida, estereo, n * 2 * sizeof(int16_t), &escritos, portMAX_DELAY);
}

static void sonar_pcm(const uint8_t *pcm, size_t bytes)
{
    int16_t pedazo[MUESTRAS_POR_VUELTA];
    size_t total = bytes / 2;
    for (size_t hecho = 0; hecho < total; ) {
        int n = (total - hecho > MUESTRAS_POR_VUELTA) ? MUESTRAS_POR_VUELTA : (int)(total - hecho);
        memcpy(pedazo, pcm + hecho * 2, n * 2);      // copia (por si el audio no esta alineado)
        mandar(pedazo, n);
        hecho += n;
    }
}

static void sonar_nota(int hz, int ms)
{
    int16_t pedazo[MUESTRAS_POR_VUELTA];
    int total = AUDIO_MUESTRAS_POR_SEGUNDO * ms / 1000;
    int suave = AUDIO_MUESTRAS_POR_SEGUNDO / 200;             // 5 ms para entrar y salir suave
    float fase = 0, paso = 2 * (float)M_PI * hz / AUDIO_MUESTRAS_POR_SEGUNDO;
    for (int hecho = 0; hecho < total; ) {
        int n = (total - hecho > MUESTRAS_POR_VUELTA) ? MUESTRAS_POR_VUELTA : total - hecho;
        for (int i = 0; i < n; i++, hecho++) {
            float env = 1;
            if (hecho < suave) env = (float)hecho / suave;
            if (total - hecho < suave) env = (float)(total - hecho) / suave;
            pedazo[i] = (hz > 0) ? (int16_t)(9000 * env * sinf(fase)) : 0;
            fase += paso;
            if (fase > 2 * (float)M_PI) fase -= 2 * (float)M_PI;
        }
        mandar(pedazo, n);
    }
}

static void tarea_audio(void *arg)
{
    trabajo_t t;
    bool amplificador = false;
    while (1) {
        if (xQueueReceive(fila, &t, pdMS_TO_TICKS(PAUSA_APAGAR_MS)) == pdTRUE) {
            if (!amplificador) {
                placa_amplificador(true);
                amplificador = true;
                vTaskDelay(pdMS_TO_TICKS(10));       // que el amplificador se despierte
            }
            if (t.pcm) sonar_pcm(t.pcm, t.bytes);
            else       sonar_nota(t.hz, t.ms);
            nivel = 0;
            __atomic_fetch_sub(&pendientes, 1, __ATOMIC_SEQ_CST);      // una cosa menos
        } else {
            // Hace un rato que no hay nada: apagamos el amplificador
            if (amplificador) {
                placa_amplificador(false);
                amplificador = false;
            }
        }
    }
}

// ─── Publico ─────────────────────────────────────────────────────────────────

void audio_iniciar(void)
{
    fila = xQueueCreate(32, sizeof(trabajo_t));
    if (!preparar_es8311()) return;
    bool hay_mics = preparar_es7210();
    preparar_i2s();
    audio_volumen(70);
    xTaskCreate(tarea_audio, "audio", 4096, NULL, 6, NULL);
    if (hay_mics) xTaskCreate(tarea_microfono, "microfono", 4096, NULL, 6, NULL);
    else          es7210 = NULL;
    ESP_LOGI(TAG, "Audio listo (%s)", hay_mics ? "parlante y microfonos" : "solo parlante");
}

void audio_volumen(int porcentaje)
{
    if (es8311 == NULL) return;
    if (porcentaje < 0) porcentaje = 0;
    if (porcentaje > 100) porcentaje = 100;
    escribir(REG_VOLUMEN, (uint8_t)(porcentaje * 0xBF / 100));
}

void audio_reproducir(const void *pcm, size_t bytes)
{
    if (fila == NULL || es8311 == NULL) return;
    trabajo_t t = { .pcm = pcm, .bytes = bytes };
    __atomic_fetch_add(&pendientes, 1, __ATOMIC_SEQ_CST);
    xQueueSend(fila, &t, portMAX_DELAY);
}

void audio_melodia(const audio_nota_t *notas, int cantidad)
{
    if (fila == NULL || es8311 == NULL) return;
    for (int i = 0; i < cantidad; i++) {
        trabajo_t t = { .pcm = NULL, .hz = notas[i].hz, .ms = notas[i].ms };
        __atomic_fetch_add(&pendientes, 1, __ATOMIC_SEQ_CST);
        xQueueSend(fila, &t, portMAX_DELAY);
    }
}

bool audio_sonando(void)
{
    return pendientes > 0;
}

void audio_esperar(void)
{
    while (pendientes > 0) vTaskDelay(pdMS_TO_TICKS(20));
}

float audio_nivel(void)
{
    return nivel;
}

// ─── Microfonos ──────────────────────────────────────────────────────────────

bool audio_mic_listo(void)
{
    return es7210 != NULL;
}

float audio_mic_nivel(void)
{
    return nivel_mic;
}

void audio_grabar_empezar(int16_t *destino, size_t max_muestras)
{
    grabando = false;
    grabacion = destino;
    max_grabar = max_muestras;
    grabadas = 0;
    grabando = (destino != NULL && max_muestras > 0);
}

bool audio_grabar_lleno(void)
{
    return grabadas >= max_grabar;
}

size_t audio_grabar_terminar(void)
{
    grabando = false;
    return grabadas;
}
