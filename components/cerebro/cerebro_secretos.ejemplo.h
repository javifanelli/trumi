// ─────────────────────────────────────────────────────────────────────────────
//  PLANTILLA de los datos del cerebro de Trumi (la Raspberry)
// ─────────────────────────────────────────────────────────────────────────────
//
//  1. Copia este archivo en la misma carpeta con el nombre:  cerebro_secretos.h
//  2. En la copia, pone la direccion de la Raspberry y la clave de su config.json.
//
//  cerebro_secretos.h esta en el .gitignore: se queda en tu compu y NUNCA se
//  sube al repo. Este archivo de ejemplo si se sube, asi que aca no pongas nada real.
//
#ifndef CEREBRO_SECRETOS_H
#define CEREBRO_SECRETOS_H

// Donde escucha el cerebro: "http://IP-DE-LA-RASPBERRY:8765" (sin "/" al final).
// Conviene la IP fija de la Raspberry; "http://nombre.local:8765" tambien puede andar.
#define CEREBRO_URL    "http://192.168.0.100:8765"

// La misma "clave" que pusiste en config.json del cerebro en la Raspberry
#define CEREBRO_CLAVE  "inventa-una-clave"

#endif // CEREBRO_SECRETOS_H
