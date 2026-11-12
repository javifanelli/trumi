# 📔 Diario de Trumi

Aca anotamos todo lo que le vamos enseñando a Trumi y lo que aprendemos en el camino.
Lo mas nuevo va arriba.

> **Para anotar algo nuevo**, copia esto arriba de todo y completalo:
>
> ```
> ## AAAA-MM-DD — Titulo
>
> **Que hicimos:** …
> **Que aprendimos:** …
> ```

---

## 2026-11-12 — Trumi tambien anda sola 🦜

**Que hicimos:** El cerebro de la Raspberry es **muy lento** (¡tardaba mas de un minuto en pensar!). Asi que ahora, cuando Trumi se prende, espera el WiFi un ratito (hasta 20 segundos) y le pregunta a su cerebro **"¿estas?"**. Si no hay red o el cerebro no contesta enseguida, Trumi **anda sola** como antes: juega al **lorito** y repite lo que le dicen, hasta que la apaguen. Y si en medio de una charla el cerebro falla, en vez de decir "¡Uy!" repite lo que escucho y sigue de lorito.

Tambien, en broma, le pedimos algo de grandes y contesto algo raro (¡y encima tardo tanto que Trumi se canso de esperar y corto!). Le enseñamos a contestar con dulzura que eso es cosa de grandes y proponer otro juego, y ahora Trumi espera hasta 150 segundos.

Y al cerebro le armamos **su propio repo** ([cerebro](https://github.com/javifanelli/cerebro)), asi en la Raspberry se actualiza con un simple `git pull`.

**Que aprendimos:**
- Antes de mandarle una carta larga a alguien, conviene preguntarle si esta en casa. Eso es lo que hace Trumi con su cerebro: una pregunta cortita al prenderse.
- Un juguete tiene que andar **siempre**, aunque falle algo que no depende de el (el WiFi, la Raspberry). Tener un "plan B" lo hace mucho mas divertido.

---

## 2026-11-12 — ¡La primera charla! 💬

**Que hicimos:** Trumi ahora se conecta al **WiFi** de la casa y habla con su **cerebro** en la Raspberry. Le dejas el dedo apoyado, le hablas, y cuando soltas el dedo pone **cara de pensar** ("Mmm...") mientras el cerebro escucha, piensa y prepara la voz. ¡Y despues **te contesta**, con la voz de Daniela y el texto en la pantalla! Si no tiene WiFi, sigue jugando al lorito.

**Que aprendimos:**
- Trumi le manda al cerebro los **numeros del sonido** por WiFi, y el cerebro le devuelve otros numeros: los de su respuesta hablada. Es como mandar una carta y recibir otra.
- Mientras el cerebro piensa (tarda unos segundos), Trumi no se queda congelada: una parte de su programa espera la respuesta y otra le mueve los ojos y los puntitos de "Mmm...".
- Las respuestas largas se **parten en renglones** para que entren en la pantalla, cortando en los espacios para no partir las palabras.

---

## 2026-11-11 — El cerebro de Trumi 🧠

**Que hicimos:** Le armamos a Trumi su **cerebro**, que vive en la **Raspberry Pi** de la casa. Cuando le llega lo que Trumi escucho, el cerebro lo **pasa a texto** (con un programa que se llama Whisper), **piensa que contestar** con una inteligencia artificial (Qwen) que sabe que es Trumi, la hija de Pitri y Flufi, y **lo convierte en voz** (con Piper, con una voz argentina llamada Daniela, un poquito mas aguda para que suene como una nena). Tambien le subimos la **sensibilidad a los microfonos**, porque habia que hablarle muy fuerte y de cerca.

**Que aprendimos:**
- Una **inteligencia artificial** de estas aprendio leyendo muchisimos textos, y asi aprendio a contestar. Le explicamos como es Trumi (su "personalidad") y que tiene que contestar corto, tierno y solo cosas para chicos.
- Todo pasa **adentro de la casa**: lo que Sofi le dice a Trumi no sale a internet.
- Para que la voz suene de nena, el cerebro la hace **mas aguda**: la habla mas despacio y despues la acelera, asi queda finita pero a velocidad normal.
- El cerebro anota las charlas en un cuaderno (`charlas.log`) para que papa y mama puedan leerlas.

---

## 2026-11-11 — ¡Trumi escucha! 🎤

**Que hicimos:** Le prendimos los **dos microfonos** a Trumi. Ahora juega al **lorito**: si le dejas el dedo apoyado, pone cara de escuchar (los ojos un poco mas grandes y **ondas de sonido** a los costados que se mueven cuando le hablas), graba lo que le decis, y cuando soltas el dedo ¡**lo repite**! Y si escucha un ruido fuerte mientras esta tranquila, pregunta "¿Me hablaste?".

**Que aprendimos:**
- Un **microfono** hace al reves que un parlante: el sonido mueve una membranita y eso se convierte en electricidad. Otro chip (el ES7210) la convierte en **numeros**: 16.000 por segundo, igual que para hablar.
- Trumi tiene **dos microfonos**, uno a cada lado. Junta lo que escuchan los dos (hace el promedio).
- Para grabar, Trumi va **guardando los numeros** en su memoria mientras escucha. Despues, para repetirlo, ¡se los manda al parlante!
- Lo mismo que hace el lorito es lo que va a hacer para **charlar**: en vez de repetirlo, le va a mandar lo que escucho a su cerebro en la Raspberry.

---

## 2026-11-11 — ¡Trumi habla! 🔊

**Que hicimos:** Le prendimos el **parlante** a Trumi. Ahora cuando se prende hace un "pling" y dice **"¡Hola! Soy Trumi"**, y despues **"¡Hola Sofi!"**. Cuando la tocas hace "bip", y cuando soltas el dedo dice "¡Jiji!", "¡Me hiciste cosquillas!" o "¡Te quiero!". Y lo mas lindo: mientras habla, ¡su **boca se abre y se cierra** con su voz!

**Que aprendimos:**
- El sonido en la computadora son **numeros**: 16.000 por segundo. Cada numero dice para donde empujar la membrana del parlante. Un chip (el ES8311) convierte esos numeros en electricidad, y un **amplificador** la hace mas fuerte para que se escuche.
- Una **nota musical** es una onda que sube y baja muchas veces por segundo: el DO sube y baja 523 veces, y cuanto mas rapido, mas aguda suena.
- La voz de Trumi por ahora es una voz de la computadora **acelerada un poquito**: asi suena mas finita, como de nena. Mas adelante va a tener su propia voz.
- Para mover la boca, Trumi mide **que tan fuerte** esta sonando lo que dice: si suena fuerte, abre mucho la boca; si suena despacito, la abre poquito.

---

## 2026-11-11 — ¡Nacio Trumi! 💜

**Que hicimos:** Empezamos a armar a **Trumi**, la hija de Pitri y Flufi. Elegimos una placa que
ya trae todo: una pantalla grande a color que se puede **tocar**, dos microfonos, un parlante y
un sensor de presencia. Le dibujamos su cara: los **ojos de papa Pitri** (grandes y redondeados),
las **pestañas de mama Flufi**, **brillitos** en los ojos y **cachetes rosados**. Juega con sus
ojos, y cuando la tocas, ¡sus ojos **siguen tu dedo**! Cuando lo soltas, se pone contenta y dice
"¡Jiji!" o "¡Me hiciste cosquillas!".

**Que aprendimos:**
- La pantalla de Trumi tiene **320 × 480 puntitos** (pixeles): ¡casi 10 veces mas que la de
  Flufi! Para guardar el dibujo entero hace falta mucha memoria, y por eso la placa tiene
  **8 MB de memoria extra** (PSRAM).
- Se le habla a la pantalla por **8 cables a la vez** (un "bus paralelo"): asi el dibujo llega
  muy rapido.
- La pantalla **tactil** es un vidrio que siente donde esta el dedo y le avisa a la placa la
  posicion: x (de izquierda a derecha) e y (de arriba a abajo).
- Algunas cosas de la placa (como la luz de la pantalla) no estan conectadas directo al cerebro:
  pasan por un chip **"expansor"**, que es como un ayudante con 16 manos extra.
