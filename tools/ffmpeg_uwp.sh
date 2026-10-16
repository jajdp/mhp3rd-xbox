#!/bin/sh
# FFmpeg 7.1.5 para el port UWP. Lo llama ffmpeg_uwp.ps1 desde el bash de MSYS2, con el entorno «store» de
# MSVC ya cargado (cl, link, INCLUDE y LIB de las apps). Uso: ffmpeg_uwp.sh <carpeta de trabajo> <hilos>
#
# Los flags de la biblioteca son los de la compilación «bundled» de Yakumo (profiles/mhp3rd/cmake/FFmpeg.cmake):
# solo avcodec y avutil, con los decodificadores ATRAC3, ATRAC3plus y H.264, sin asm y LGPL. Lo que añade el port
# es lo de la guía de FFmpeg para WinRT: el toolchain de MSVC, WINAPI_FAMILY_APP y enlazar con /APPCONTAINER
# contra WindowsApp.lib. Al compilar «para otra máquina», el compilador anfitrión por defecto es gcc, que aquí
# no está: también es cl.exe. El prefijo es neutro (/ffmpeg) y se instala por DESTDIR, para que ninguna ruta de esta
# PC quede escrita en la línea de configuración que guarda la biblioteca.
set -e
trabajo="$1"
hilos="${2:-16}"
mkdir -p "$trabajo/build"
cd "$trabajo/build"
../ffmpeg-7.1.5/configure \
    --toolchain=msvc --arch=x86_64 --target-os=win32 --enable-cross-compile \
    --host-cc=cl.exe \
    --prefix=/ffmpeg \
    --extra-cflags="-MD -DWINAPI_FAMILY=WINAPI_FAMILY_APP -D_WIN32_WINNT=0x0A00" \
    --extra-ldflags="-APPCONTAINER WindowsApp.lib" \
    --enable-shared --disable-static --disable-programs --disable-doc \
    --disable-avdevice --disable-avformat --disable-avfilter --disable-swscale --disable-swresample \
    --disable-network --disable-autodetect --disable-everything \
    --enable-decoder=atrac3,atrac3p,h264 \
    --disable-x86asm --disable-debug
make -j"$hilos"
make install DESTDIR="$trabajo/install"
