#include "tactil.h"
#include "placa.h"
#include "board_config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "TACTIL";

/*
 * El FT6336U es el chip que siente los dedos sobre el vidrio. Se le pregunta
 * por I2C y contesta con 5 numeros a partir de su registro 0x02:
 *   [0] cuantos dedos hay apoyados (0, 1 o 2)
 *   [1] y [2] la X del primer dedo (12 bits: los 4 de abajo de [1] y todo [2])
 *   [3] y [4] la Y del primer dedo (igual)
 */
#define REG_ESTADO      0x02
#define REVISAR_CADA_MS 20

static i2c_master_dev_handle_t chip;
static volatile bool tocando = false, toque_nuevo = false;
static volatile int ultimo_x, ultimo_y, toque_x, toque_y;

static void tarea_tactil(void *arg)
{
    bool antes = false;
    while (1) {
        uint8_t reg = REG_ESTADO, d[5];
        if (i2c_master_transmit_receive(chip, &reg, 1, d, sizeof(d), 50) == ESP_OK) {
            int dedos = d[0] & 0x0F;
            bool ahora = (dedos >= 1 && dedos <= 2);
            if (ahora) {
                int x = ((d[1] & 0x0F) << 8) | d[2];
                int y = ((d[3] & 0x0F) << 8) | d[4];
                if (TACTIL_INVERTIR_X) x = LCD_H_RES - 1 - x;
                if (TACTIL_INVERTIR_Y) y = LCD_V_RES - 1 - y;
                ultimo_x = x;
                ultimo_y = y;
                if (!antes) {                       // recien apoyo el dedo: ¡un toque!
                    toque_x = x;
                    toque_y = y;
                    toque_nuevo = true;
                    ESP_LOGI(TAG, "Toque en (%d, %d)", x, y);
                }
            }
            tocando = ahora;
            antes = ahora;
        }
        vTaskDelay(pdMS_TO_TICKS(REVISAR_CADA_MS));
    }
}

void tactil_iniciar(void)
{
    i2c_device_config_t dev = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = TACTIL_DIRECCION,
        .scl_speed_hz    = I2C_HZ,
    };
    if (i2c_master_bus_add_device(placa_bus_i2c(), &dev, &chip) != ESP_OK) {
        ESP_LOGE(TAG, "No pude agregar el tactil al bus I2C");
        return;
    }
    if (i2c_master_probe(placa_bus_i2c(), TACTIL_DIRECCION, 100) != ESP_OK)
        ESP_LOGW(TAG, "El tactil no contesta en la direccion 0x%02X", TACTIL_DIRECCION);
    else
        ESP_LOGI(TAG, "Tactil listo");
    xTaskCreate(tarea_tactil, "tactil", 3072, NULL, 5, NULL);
}

bool tactil_tocando(int *x, int *y)
{
    if (tocando) {
        if (x) *x = ultimo_x;
        if (y) *y = ultimo_y;
    }
    return tocando;
}

bool tactil_hay_toque_nuevo(void)
{
    return toque_nuevo;
}

bool tactil_se_toco(int *x, int *y)
{
    if (!toque_nuevo) return false;
    toque_nuevo = false;
    if (x) *x = toque_x;
    if (y) *y = toque_y;
    return true;
}
