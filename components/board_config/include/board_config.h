#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/*
 * ═════════════════════════════════════════════════════════════════════════════
 *  TRUMI — Conexiones de la placa "ESP32 Agent Dev Kit" (Wireless-Tag)
 * ═════════════════════════════════════════════════════════════════════════════
 *
 *  Todo viene armado en la placa: no hay que cablear nada. Los pines salen del
 *  esquematico y del codigo de ejemplo del fabricante (ver datasheets/).
 *
 *  Placa: ESP32-S3 N16R8 (16 MB de flash, 8 MB de PSRAM)
 *
 *  ┌────────────────────────────┬──────────────────────────────────────────────┐
 *  │ Parte                      │ Pines                                        │
 *  ├────────────────────────────┼──────────────────────────────────────────────┤
 *  │ Bus I2C (compartido)       │ SDA = GPIO 5, SCL = GPIO 4                   │
 *  │   - Expansor AW9523        │   direccion 0x59                             │
 *  │   - Tactil FT6336U         │   direccion 0x38, aviso (INT) en GPIO 6      │
 *  │   - Audio ES8311 / ES7210  │   (los vamos a usar en la etapa 2)           │
 *  ├────────────────────────────┼──────────────────────────────────────────────┤
 *  │ Pantalla 3,5" ST7796       │ Bus paralelo de 8 bits ("8080"):             │
 *  │ 320 x 480, a color         │   D0..D7 = 9, 3, 8, 18, 17, 16, 15, 7        │
 *  │                            │   WR = 10, RS (dato/comando) = 45            │
 *  │                            │   Luz de fondo: por el expansor AW9523       │
 *  ├────────────────────────────┼──────────────────────────────────────────────┤
 *  │ Audio (I2S, etapa 2)       │ MCLK 47, BCLK 14, LRCK 13, sale 12, entra 21 │
 *  │                            │ Amplificador: pin P1_5 del AW9523            │
 *  ├────────────────────────────┼──────────────────────────────────────────────┤
 *  │ Boton BOOT (atras)         │ GPIO 0                                       │
 *  │ USB                        │ D- = 19, D+ = 20                             │
 *  │ Libres (conector mikroBUS) │ 1, 2, 11, 38, 39, 40, 41, 42, 46, 48         │
 *  └────────────────────────────┴──────────────────────────────────────────────┘
 */

#include "driver/gpio.h"

// ─── Bus I2C ──────────────────────────────────────────────────────────────────
#define PIN_I2C_SDA        GPIO_NUM_5
#define PIN_I2C_SCL        GPIO_NUM_4
#define I2C_HZ             (400 * 1000)

// ─── Expansor de pines AW9523 ────────────────────────────────────────────────
// Maneja cosas que no estan conectadas directo al ESP32. Sus pines se llaman
// P0_0..P0_7 y P1_0..P1_7. Aca los escribimos como 0xPN (puerto P, numero N).
#define AW9523_DIRECCION   0x59
#define AW_LUZ_FONDO_1     0x10   // P1_0 .. P1_3, P0_0 y P0_1: la luz de la pantalla
#define AW_LUZ_FONDO_2     0x11   // (se manejan como LEDs, con brillo)
#define AW_LUZ_FONDO_3     0x12
#define AW_LUZ_FONDO_4     0x13
#define AW_LUZ_FONDO_5     0x00
#define AW_LUZ_FONDO_6     0x01
#define AW_AMPLIFICADOR    0x15   // P1_5: prende el amplificador del parlante
#define AW_RESET_PLACA     0x16   // P1_6: reset de la pantalla y del audio

// ─── Pantalla ST7796 (bus paralelo de 8 bits) ────────────────────────────────
#define PIN_LCD_D0         GPIO_NUM_9
#define PIN_LCD_D1         GPIO_NUM_3
#define PIN_LCD_D2         GPIO_NUM_8
#define PIN_LCD_D3         GPIO_NUM_18
#define PIN_LCD_D4         GPIO_NUM_17
#define PIN_LCD_D5         GPIO_NUM_16
#define PIN_LCD_D6         GPIO_NUM_15
#define PIN_LCD_D7         GPIO_NUM_7
#define PIN_LCD_WR         GPIO_NUM_10
#define PIN_LCD_RS         GPIO_NUM_45
#define LCD_H_RES          320       // ancho (la pantalla esta parada)
#define LCD_V_RES          480       // alto
#define LCD_PCLK_HZ        (20 * 1000 * 1000)   // velocidad del bus (el fabricante usa 40 MHz)

// Orientacion (MADCTL). El fabricante usa 0x48. Si la imagen se ve espejada o
// dada vuelta, probar: 0x08, 0x88 o 0xC8 (el 0x08 del final es el orden de colores BGR).
#define LCD_MADCTL         0x48

// ─── Audio (I2S): conversor ES8311 (parlante) y ES7210 (microfonos) ──────────
#define PIN_I2S_MCLK       GPIO_NUM_47
#define PIN_I2S_BCLK       GPIO_NUM_14
#define PIN_I2S_LRCK       GPIO_NUM_13
#define PIN_I2S_DOUT       GPIO_NUM_12    // del ESP32 al ES8311 (lo que suena)
#define PIN_I2S_DIN        GPIO_NUM_21    // de los microfonos al ESP32 (etapa 3)

// ─── Tactil FT6336U ──────────────────────────────────────────────────────────
#define TACTIL_DIRECCION   0x38
#define PIN_TACTIL_INT     GPIO_NUM_6
// Si al tocar, el punto queda espejado respecto del dedo, cambiar a 1
#define TACTIL_INVERTIR_X  0
#define TACTIL_INVERTIR_Y  0

// ─── Botones ─────────────────────────────────────────────────────────────────
#define PIN_BOTON_BOOT     GPIO_NUM_0    // apretado = 0

#endif // BOARD_CONFIG_H
