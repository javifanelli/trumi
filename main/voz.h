#ifndef VOZ_H
#define VOZ_H

// Las frases que Trumi tiene grabadas adentro del programa (en main/voz/).
// Son audio crudo: 16 bits, mono, 16 kHz.
// (Por ahora las genero con la voz de Windows. Cuando tenga su cerebro en la
//  Raspberry, va a hablar con su propia voz y decir cualquier cosa.)

typedef enum {
    VOZ_HOLA_SOY_TRUMI,
    VOZ_HOLA_SOFI,
    VOZ_HOLA,
    VOZ_JIJI,
    VOZ_COSQUILLAS,
    VOZ_TE_QUIERO,
    VOZ_OTRA_VEZ,
    CANT_VOCES
} voz_t;

// Pone a sonar la frase (no espera a que termine)
void voz_decir(voz_t frase);

#endif // VOZ_H
