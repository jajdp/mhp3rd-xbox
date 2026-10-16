# Cómo funciona, y por qué cada pieza está ahí

*(English: [HOW-IT-WORKS.md](HOW-IT-WORKS.md))*

## El problema de fondo: una Xbox no tiene Vulkan

Yakumo dibuja **solo con Vulkan**. Una aplicación UWP en una Xbox tiene Direct3D 12 y nada más: no
hay controlador de Vulkan, ni forma de instalarlo.

La salida es **Dozen**, la capa de Mesa que implementa Vulkan sobre D3D12. Y eso cambia la forma del
trabajo: no es un port, son dos capas que tienen que funcionar a la vez —el juego sobre Vulkan, y
Vulkan sobre D3D12 dentro de un contenedor—.

Por eso el trabajo se hizo **en cuatro etapas, en este orden**, y es la parte más reutilizable de
todo esto:

| | |
|---|---|
| 1 | El juego en el **PC** con Dozen, no con el Vulkan del fabricante. Si no funciona ahí, lo demás no tiene sentido |
| 2 | Una **aplicación mínima** con el Dozen de UWP, instalada en la consola. Crea la instancia, elige dispositivo, dibuja y dice qué pasó. Es `src/dozen/` de este repositorio |
| 3 | Compilar Yakumo **desde el código**, no usar la release |
| 4 | El port UWP propiamente |

Saltarse la etapa 2 es lo que convierte un port en una semana de dar palos de ciego: cuando el juego
no dibuja, no hay forma de saber si es el juego, el empaquetado o la capa.

## La pantalla en negro, y una hipótesis mía que era falsa

Con el port ya instalado, el juego **corría** —se oía, avanzaba— y la pantalla estaba **en negro**.

La primera hipótesis fue mía y era **equivocada**: que una aplicación de la Tienda tiene un tope de
memoria de vídeo y lo estábamos pasando. Costó una tarde y no era eso.

Lo que lo encontró fue dejar de suponer y **consultar los formatos uno a uno**. El juego pedía
`D32_SFLOAT` para el buffer de profundidad, y en esa capa ese formato devolvía **0 bytes por
píxel**: la profundidad no existía, así que todo quedaba descartado. Con **`D24S8`** se ve.

La lección, que vale para cualquier port sobre una capa de traducción: **lo que el formato dice que
soporta y lo que de verdad devuelve pueden no coincidir**, y preguntarlo cuesta diez líneas.

## La música y las películas: FFmpeg para UWP

El juego las quiere, y FFmpeg **no se publica compilado para UWP**. Hay que compilarlo: con MSYS2,
configurado contra el CRT «store» y sin las partes que un App Container no permite.

Eso es `tools/ffmpeg_uwp.sh` (lo que corre dentro de MSYS2) y `tools/ffmpeg_uwp.ps1` (lo que lo
llama desde Windows). **Es la pieza más reutilizable de este repositorio**: cualquier otro port UWP
que necesite FFmpeg puede usarla igual, cambiando solo el nombre de las bibliotecas que quiera.

## El ISO, en trozos

El Device Portal no acepta el ISO de una vez. Va en trozos de 90 MiB (`tools/partir_iso.py`), y los
datos del juego se preparan aparte (`tools/datos_consola.py`) porque el port los lee de
`LocalState`.

Eso tiene una consecuencia buena: **la versión traducida no necesita otro paquete**. Los mods de
traducción se suben al `LocalState` y el juego los lee; el `.msix` es el mismo.

## Lo que falta

- **La suspensión del modo desarrollador**: al volver de Home, el juego empieza de nuevo.
- La versión en español se instala subiendo sus mods al `LocalState`, pero conviene tenerla como
  paquete hermano con su propia identidad para no mezclar partidas.
