# Aviso

*(English: [NOTICE.md](NOTICE.md))*

## Sin afiliación

Esto es un proyecto de aficionado, no oficial y sin ánimo de lucro. **No está afiliado a Capcom,
Sony ni Microsoft**, ni cuenta con su respaldo, ni lo está con
[TeamGDB](https://github.com/TeamGDB/Yakumo), autores de Yakumo, ni con el proyecto Mesa.
*Monster Hunter Portable 3rd*, *Xbox* y todos los nombres y marcas relacionados son de sus
respectivos propietarios.

Aquí no se vende nada, y nada de esto está monetizado.

## Qué no distribuye este repositorio

Ni ISO. Ni datos, recursos, música o películas del juego. Ni ejecutable compilado. Ni `.msix`. Ni
partidas. Ni mosaicos del paquete. Ni ningún binario de terceros: ni Mesa, ni Dozen, ni SDL3, ni el
FFmpeg que esto compila —el guion que lo compila está aquí; el resultado, no—.

## Qué contiene de trabajo de otros

**Parches contra Yakumo**, que es **MIT**. El script de parcheo cita las líneas de su código que
cada cambio comprueba antes de escribir, para fallar en seguro en vez de estropear un árbol que no
reconoce. La MIT lo permite, y este repositorio se publica bajo
[esa misma licencia](LICENSE).

**`src/dozen/`** es propio: una aplicación Vulkan mínima escrita para averiguar si la capa Dozen de
Mesa arranca dentro de un App Container, antes de gastar tiempo en el juego.

**El guion de FFmpeg** configura y compila FFmpeg desde su propio código, contra el CRT de UWP.
FFmpeg es LGPL/GPL según cómo se configure; lo que hay aquí es una receta de compilación, y los
binarios que salgan son tuyos y llevan los términos de FFmpeg.

**Aquí no hay capturas de pantalla.**

## Retirada y contacto

Si tienes derechos sobre este material y quieres que algo de aquí se quite o se cambie, escribe a
**jajdpmail@gmail.com**. Las peticiones de quien tiene los derechos se atienden sin discutir.
