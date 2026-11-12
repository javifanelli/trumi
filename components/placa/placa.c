#include "placa.h"
#include "board_config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "PLACA";

/*
 * El expansor AW9523 es un chip con 16 pines extra que se manejan por I2C.
 * Adentro tiene "registros" (cajoncitos numerados). Los que usamos:
 *   0x02 / 0x03   salida del puerto 0 / 1 (1 = pin en alto)
 *   0x04 / 0x05   direccion del puerto 0 / 1 (0 = salida, 1 = entrada)
 *   0x10          numero de chip (el AW9523 contesta 0x23)
 *   0x11          control: modo del puerto 0 y corriente maxima de los LEDs
 *   0x12 / 0x13   modo de cada pin del puerto 0 / 1 (0 = LED con brillo, 1 = pin comun)
 *   0x20 .. 0x2F  brillo de cada LED (0 a 255): 0x20..0x23 = P1_0..P1_3,
 *                 0x24..0x2B = P0_0..P0_7, 0x2C..0x2F = P1_4..P1_7
 *   0x7F          reset (escribiendo 0)
 * (Mapa de la hoja de datos del AW9523B: verificarlo con la placa en mano.)
 */
#define REG_SALIDA_P0   0x02
#define REG_SALIDA_P1   0x03
#define REG_DIRECCION_P0 0x04
#define REG_DIRECCION_P1 0x05
#define REG_ID          0x10
#define REG_CONTROL     0x11
#define REG_MODO_P0     0x12
#define REG_MODO_P1     0x13
#define REG_RESET       0x7F

static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t expansor;
static uint8_t salida_p1 = 0;        // como dejamos las salidas del puerto 1

static const uint8_t luces[6] = {
    AW_LUZ_FONDO_1, AW_LUZ_FONDO_2, AW_LUZ_FONDO_3, AW_LUZ_FONDO_4, AW_LUZ_FONDO_5, AW_LUZ_FONDO_6,
};

static esp_err_t escribir(uint8_t registro, uint8_t valor)
{
    uint8_t buf[2] = { registro, valor };
    return i2c_master_transmit(expansor, buf, 2, 100);
}

static esp_err_t leer(uint8_t registro, uint8_t *valor)
{
    return i2c_master_transmit_receive(expansor, &registro, 1, valor, 1, 100);
}

// El registro de brillo de un pin (0xPN: puerto P, numero N)
static uint8_t registro_brillo(uint8_t pin)
{
    int puerto = pin >> 4, n = pin & 0x0F;
    if (puerto == 0) return 0x24 + n;                 // P0_0..P0_7
    return (n < 4) ? 0x20 + n : 0x2C + (n - 4);       // P1_0..P1_3 y P1_4..P1_7
}

static void salida_p1_pin(uint8_t pin, bool alto)
{
    if (alto) salida_p1 |= (uint8_t)(1 << (pin & 0x0F));
    else      salida_p1 &= (uint8_t)~(1 << (pin & 0x0F));
    escribir(REG_SALIDA_P1, salida_p1);
}

void placa_iniciar(void)
{
    i2c_master_bus_config_t cfg = {
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .i2c_port          = 0,
        .sda_io_num        = PIN_I2C_SDA,
        .scl_io_num        = PIN_I2C_SCL,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &bus));

    i2c_device_config_t dev = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = AW9523_DIRECCION,
        .scl_speed_hz    = I2C_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev, &expansor));

    escribir(REG_RESET, 0x00);                         // arranca de cero
    vTaskDelay(pdMS_TO_TICKS(5));
    uint8_t id = 0;
    if (leer(REG_ID, &id) != ESP_OK) {
        ESP_LOGE(TAG, "El expansor AW9523 no contesta (I2C SDA=%d, SCL=%d)", PIN_I2C_SDA, PIN_I2C_SCL);
        return;
    }
    ESP_LOGI(TAG, "Expansor AW9523 encontrado (id 0x%02X)", id);

    // Puerto 0 "empuja" (push-pull) y los LEDs con la corriente maxima
    escribir(REG_CONTROL, 0x10);

    // Que pines son LEDs (luz de la pantalla) y cuales salidas comunes
    uint8_t modo_p0 = 0xFF, modo_p1 = 0xFF, dir_p0 = 0xFF, dir_p1 = 0xFF;
    for (int i = 0; i < 6; i++) {
        uint8_t bit = (uint8_t)(1 << (luces[i] & 0x0F));
        if (luces[i] >> 4) { modo_p1 &= ~bit; dir_p1 &= ~bit; }
        else               { modo_p0 &= ~bit; dir_p0 &= ~bit; }
    }
    dir_p1 &= (uint8_t)~((1 << (AW_AMPLIFICADOR & 0x0F)) | (1 << (AW_RESET_PLACA & 0x0F)));
    escribir(REG_MODO_P0, modo_p0);
    escribir(REG_MODO_P1, modo_p1);
    escribir(REG_DIRECCION_P0, dir_p0);
    escribir(REG_DIRECCION_P1, dir_p1);
    escribir(REG_SALIDA_P0, 0x00);

    // Reset de la pantalla y del audio: un ratito en bajo y despues en alto.
    // El amplificador arranca apagado (para que no haga ruido).
    salida_p1 = 0;
    escribir(REG_SALIDA_P1, salida_p1);
    vTaskDelay(pdMS_TO_TICKS(20));
    salida_p1_pin(AW_RESET_PLACA, true);
    vTaskDelay(pdMS_TO_TICKS(120));

    placa_brillo(0);                                   // la luz se prende cuando hay algo dibujado
}

i2c_master_bus_handle_t placa_bus_i2c(void)
{
    return bus;
}

void placa_brillo(int porcentaje)
{
    if (expansor == NULL) return;
    if (porcentaje < 0) porcentaje = 0;
    if (porcentaje > 100) porcentaje = 100;
    uint8_t valor = (uint8_t)(porcentaje * 255 / 100);
    for (int i = 0; i < 6; i++) escribir(registro_brillo(luces[i]), valor);
}

void placa_amplificador(bool prendido)
{
    if (expansor == NULL) return;
    salida_p1_pin(AW_AMPLIFICADOR, prendido);
}
