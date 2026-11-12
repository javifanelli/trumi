#include "voz.h"
#include "audio.h"

#include <stdint.h>

// Los archivos de main/voz/ se guardan adentro del programa (ver EMBED_FILES en
// main/CMakeLists.txt). Para cada archivo, el programa "sabe" donde empieza y
// donde termina, con estos nombres especiales:
#define FRASE(nombre)                                                              \
    extern const uint8_t nombre##_inicio[] asm("_binary_" #nombre "_pcm_start");  \
    extern const uint8_t nombre##_fin[]    asm("_binary_" #nombre "_pcm_end");

FRASE(hola_soy_trumi)
FRASE(hola_sofi)
FRASE(hola)
FRASE(jiji)
FRASE(cosquillas)
FRASE(te_quiero)
FRASE(otra_vez)

void voz_decir(voz_t frase)
{
    static const struct { const uint8_t *inicio, *fin; } frases[CANT_VOCES] = {
        [VOZ_HOLA_SOY_TRUMI] = { hola_soy_trumi_inicio, hola_soy_trumi_fin },
        [VOZ_HOLA_SOFI]      = { hola_sofi_inicio,      hola_sofi_fin },
        [VOZ_HOLA]           = { hola_inicio,           hola_fin },
        [VOZ_JIJI]           = { jiji_inicio,           jiji_fin },
        [VOZ_COSQUILLAS]     = { cosquillas_inicio,     cosquillas_fin },
        [VOZ_TE_QUIERO]      = { te_quiero_inicio,      te_quiero_fin },
        [VOZ_OTRA_VEZ]       = { otra_vez_inicio,       otra_vez_fin },
    };
    if ((unsigned)frase >= CANT_VOCES) return;
    audio_reproducir(frases[frase].inicio, (size_t)(frases[frase].fin - frases[frase].inicio));
}
