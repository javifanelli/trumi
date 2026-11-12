# 💜 Trumi

**Trumi es la hija de Pitri y Flufi.** Tiene los ojos de su papa Pitri, las pestañas de su
mama Flufi, brillitos en los ojos y cachetes rosados. Vive en una pantalla grande a color,
¡ya **habla** y **escucha**! Y muy pronto va a poder **charlar** de verdad.

La armamos entre Sofia y su papa, de a poquito, como hicimos con Pitri y Flufi.

```
        \|/                         \|/
      ███████████            ███████████
     ███ ◯◯ ██████          ███ ◯◯ ██████     ← ojos con brillitos
     █████ ◦ █████          █████ ◦ █████
     █████████████          █████████████
      ███████████            ███████████
     (( rosado ))            (( rosado ))      ← cachetes
                   \______/                    ← sonrisa
```

---

## ✨ Que sabe hacer Trumi (por ahora)

| Cuando… | Trumi… |
|---|---|
| Se prende | Hace un "pling" y dice **"¡Hola! Soy Trumi"** (¡con su voz!) y despues **"¡Hola Sofi!"** con saltitos |
| Esta tranquila | Juega con sus ojos: mira para todos lados, pasea, parpadea, guiña un ojo, se pone contenta |
| La **tocas** | Hace "bip" y ¡sus ojos **siguen tu dedo**! Cuando lo soltas, se pone contenta y **dice** algo: "¡Jiji!", "¡Me hiciste cosquillas!", "¡Te quiero!"… |
| Habla | ¡Su **boca se abre y se cierra** con lo que dice! |
| **Dejas el dedo apoyado** | Juega al **lorito**: pone cara de escuchar (con ondas de sonido a los costados) y graba lo que le decis mientras tengas el dedo apoyado. Cuando lo soltas, ¡**lo repite**! |
| Escucha un ruido fuerte | Pone cara de escuchar y pregunta **"¿Me hablaste?"** |

## 🗺️ Lo que viene

1. ✅ **La cara y el tactil**
2. ✅ **Que hable**: frases grabadas y sonidos por su parlante, con la boca moviendose
3. ✅ **Que escuche**: el lorito y "¿Me hablaste?"
4. ✅ **Su cerebro**: un modelo de inteligencia artificial en la Raspberry Pi de casa (ver [cerebro/](cerebro/))
5. 💬 **La primera charla**: tocarla, hablarle ¡y que conteste!
6. 👨‍👩‍👧 **La familia**: que se salude y charle con Pitri y Flufi

---

## 🧩 De que esta hecha Trumi

Usa una placa que ya viene armada, con todo adentro: la **ESP32 Agent Dev Kit** de
Wireless-Tag (las hojas de datos estan en [datasheets/](datasheets/)).

| Parte | Para que sirve |
|---|---|
| **ESP32-S3** con 8 MB de memoria extra (PSRAM) | Es el **cerebro chiquito**: dibuja la cara, escucha y habla |
| **Pantalla de 3,5" a color** (320×480) | Es la **cara** |
| **Pantalla tactil** | Para que Sofi la **toque** |
| **Dos microfonos** | Para **escuchar** |
| **Parlante** | Para **hablar** |
| **Sensor de presencia** | Para darse cuenta cuando alguien se acerca (mas adelante) |

Las conexiones estan en
[components/board_config/include/board_config.h](components/board_config/include/board_config.h).

## 🎛️ Cambia como es Trumi

En [main/trumi_config.h](main/trumi_config.h) estan los colores (ojos, pestañas, cachetes,
boca, fondo), el brillo de la pantalla, el **volumen** y los mensajes. Cada color se arma con tres numeros de
0 a 255: **cuanto rojo, cuanto verde y cuanto azul**.

Y en [components/cara/cara.c](components/cara/cara.c), arriba de todo, esta la forma de la
cara: el tamaño de los ojos, que tan redondos son, que tan separados estan, hasta donde miran…

---

## 🧑‍🔧 Para grandes

- **ESP-IDF 6.0**, target `esp32s3`. La configuracion basica (16 MB de flash, PSRAM octal a
  80 MHz, 240 MHz, particion de 3 MB) esta en `sdkconfig.defaults`. Si ya existe un `sdkconfig`
  viejo, hay que regenerarlo (`idf.py set-target esp32s3`, o borrar `sdkconfig`) para que la tome.
- Compilar y cargar: `idf.py build` y `idf.py -p COMx flash monitor` (el USB-C de abajo es el
  del chip CH343P).
- Componentes:
  - `board_config`: pines.
  - `placa`: bus I2C compartido y expansor AW9523 (luz de fondo con brillo, amplificador, reset).
  - `display`: ST7796 por bus paralelo de 8 bits (`esp_lcd` i80), hoja RGB565 de 300 KB en
    PSRAM mandada por DMA; mismas funciones de dibujo que Flufi. Detecta cual de las dos
    versiones de pantalla tiene la placa (D0/D3 con pull-up, como el codigo del fabricante).
  - `tactil`: FT6336U por I2C (0x38), consultado cada 20 ms.
  - `cara`: los ojos y sus juegos; la boca se abre con el nivel de la voz (`cara_set_voz`).
  - `audio`: ES8311 por I2C (0x18/0x19) + I2S std a 16 kHz, 16 bits, MCLK 256×fs; una tarea
    reproduce una fila de audios y notas, prende el amplificador (AW9523 P1_5) solo mientras
    suena y mide el volumen para la boca.
  - Microfonos: ES7210 por I2C (0x40 a 0x43), en el mismo I2S0 en full duplex (I2S normal,
    16 bits, mic 1 a la izquierda y mic 2 a la derecha; se promedian), ganancia 30 dB
    (`MIC_GANANCIA` en audio.c). Una tarea lee todo el tiempo, mide el nivel y graba en un
    buffer en PSRAM cuando se le pide (`audio_grabar_empezar/terminar`).
- Las frases grabadas estan en `main/voz/*.pcm` (16 bits, mono, 16 kHz, sin encabezado) y se
  meten en el programa con `EMBED_FILES`. Por ahora salen de la voz "Helena" de Windows,
  grabada a 12 kHz y reproducida a 16 kHz para que suene mas aguda. La voz de verdad va a
  venir de Piper en la Raspberry.
- El codigo de ejemplo del fabricante queda en `datasheets/codigo_fabricante/` solo para
  consultar (esta en el `.gitignore`: tiene sus propias licencias).
- **Cosas a verificar con la placa en mano** (ver docs/DIARIO.md): la orientacion de la
  pantalla (`LCD_MADCTL`), la del tactil (`TACTIL_INVERTIR_X/Y`) y el mapa de registros del
  AW9523 (si la pantalla queda negra, empezar por ahi).

## 📔 Diario

Lo que vamos haciendo esta en [docs/DIARIO.md](docs/DIARIO.md).
