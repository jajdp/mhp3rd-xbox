# Monster Hunter Portable 3rd HD en la Xbox — el port UWP

Cómo coger la recompilación estática de *Monster Hunter Portable 3rd HD* (PSP, 2010) que hace
**Yakumo** ([TeamGDB](https://github.com/TeamGDB/Yakumo)) y hacerla correr **en una Xbox Series en
modo desarrollador**, como un paquete UWP que compilas y firmas tú.

*(English: [README.md](README.md))*

No es una teoría: el juego arranca, se juega con mando, **y suena**, con música y películas.

## Lo que hace especial a este port: la Xbox no tiene Vulkan

Yakumo dibuja **solo con Vulkan**. Y una aplicación UWP en una Xbox no tiene Vulkan: tiene
Direct3D 12 y nada más.

La salida es **Dozen**, la capa de Mesa que implementa Vulkan sobre D3D12. Eso convierte el port en
un problema de dos capas en vez de uno: hay que llevar a UWP el juego **y** el Vulkan que el juego
cree que tiene. Por eso este repositorio trae, además de la receta, **la aplicación mínima que
probó Dozen en la consola antes de intentar el juego** (`src/dozen/`): una prueba de 45 KB que
dibuja un triángulo y dice si la capa está viva. Si Dozen no arranca ahí, no hay port que intentar.

## Qué es este repositorio

| Pieza | Qué es |
|---|---|
| `tools/parchar_uwp.py` | Los cambios del port sobre el árbol de Yakumo, con anclas comprobadas |
| `tools/construir-uwp.ps1` | El guion de construcción: compila, monta el paquete y firma |
| `tools/ffmpeg_uwp.ps1` y `.sh` | **FFmpeg compilado para UWP**, que es lo que da la música y las películas. Es el trozo más reutilizable de aquí: cualquier port UWP que necesite FFmpeg puede usarlo igual |
| `tools/compilar-pc.ps1` | Compilar Yakumo desde el código, que es el paso previo |
| `tools/partir_iso.py`, `datos_consola.py` | El ISO en trozos y los datos del juego para la consola |
| `src/dozen/` | La aplicación mínima que prueba Dozen en la consola |

## Qué **no** es

Aquí no hay **ISO, ni datos del juego, ni ejecutable, ni `.msix`, ni mosaicos, ni Mesa, ni FFmpeg
compilado**. Necesitas tu propia copia del juego y Yakumo compilando antes de empezar. Está
detallado en [`NOTICE.es.md`](NOTICE.es.md).

## Qué hace falta antes

| | |
|---|---|
| **Yakumo** | [TeamGDB/Yakumo](https://github.com/TeamGDB/Yakumo); esto se hizo con la `v0.6.0-alpha.4`, y **compilando en tu PC primero** |
| **Tu propia copia** del juego | Un ISO de *Monster Hunter Portable 3rd HD* |
| **Mesa con Dozen**, para escritorio y para UWP | La capa Vulkan-sobre-D3D12. Sin ella no hay dibujo |
| **MSYS2** | Para compilar FFmpeg para UWP (`tools/ffmpeg_uwp.sh`) |
| **Visual Studio 2022** con el componente de **UWP para C++** | Compilar y empaquetar |
| **SDL3** para Visual C | Ventana y entrada |
| **Python 3**, una Xbox en modo desarrollador y un certificado | Los scripts, la consola y la firma |

## Cómo va

```powershell
# las rutas, una vez por sesión (docs/INSTALL.es.md §2)
$env:MHP3RD_ARBOL = "C:\mh\yakumo"; $env:MHP3RD_PKG = "C:\mh\paquete"
$env:MHP3RD_CERT  = "<la huella de tu certificado>"

python tools\parchar_uwp.py                    # los cambios del port
powershell -ExecutionPolicy Bypass -File tools\construir-uwp.ps1
```

Paso a paso, con el lado de la consola: **[`docs/INSTALL.es.md`](docs/INSTALL.es.md)**.

## Qué hubo que resolver

El detalle está en **[`docs/HOW-IT-WORKS.es.md`](docs/HOW-IT-WORKS.es.md)**.

| | Síntoma | Causa |
|---|---|---|
| 1 | No hay Vulkan en la consola | **Dozen**, la capa de Mesa sobre D3D12. Y antes de intentar el juego, la aplicación mínima de `src/dozen/` que comprueba que está viva |
| 2 | **La pantalla en negro**, con el juego corriendo por detrás | El formato de profundidad: `D32_SFLOAT` devolvía **0 bytes** por píxel en esa capa. Con `D24S8` se ve. ⚠ La primera hipótesis —que fuera el tope de memoria de una app de la Tienda— era **mía y era falsa**, y costó una tarde: lo que lo encontró fue consultar los formatos uno a uno |
| 3 | Sin música ni películas | FFmpeg no venía para UWP: se compila con MSYS2 contra el CRT «store» (`tools/ffmpeg_uwp.sh`) |
| 4 | El ISO no sube por el Device Portal | En trozos de 90 MiB, que el juego une |

## Créditos y licencia

Port y herramientas de **Recompilaciones**, bajo la [MIT](LICENSE), la misma de
[Yakumo](https://github.com/TeamGDB/Yakumo), que es el código que esto parchea.

*Monster Hunter Portable 3rd* es © Capcom. Este proyecto no está afiliado a ellos, ni a Microsoft,
ni a TeamGDB, y no distribuye nada que les pertenezca. Quien tenga derechos puede escribir a
**jajdpmail@gmail.com**.
