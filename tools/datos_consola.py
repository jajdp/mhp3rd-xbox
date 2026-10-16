"""Prepara los datos que el port necesita en el LocalState de la consola, sin tocar los del usuario.

    py -3.13 datos_consola.py [carpeta de salida] [datos de la PC]
    (las dos carpetas se le pasan por argumento)

Deja en la carpeta:
- EBOOT.ELF, copia del de pc\\datos\\ (el ejecutable de la PSP ya descifrado, técnico §4.1);
- settings.ini, el de pc\\datos\\ con lo que cambia en la consola: la imagen del disco con ruta
  relativa (disc.iso, junto al settings.ini en LocalState), pantalla completa, sin ratón ni controles
  táctiles, y la resolución interna en ×2 para el primer arranque;
- entorno.txt, las variables de entorno que uwp_app.cpp fija al arrancar (en UWP no llegan de fuera);
- mods.ini y mods\\, si los datos de la PC los tienen: los de la versión en español, que pasa su pc\\datos\\.
La ISO no se copia: se sube en partes (partir_iso.py).
"""
import os
import pathlib
import shutil
import sys

SUB = pathlib.Path(__file__).resolve().parent.parent.parent
SALIDA = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else r"<MHP3RD_PKG>\datos")
DATOS_PC = pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else SUB / "pc" / "datos"

CAMBIOS = {
    "disc_image": "disc.iso",
    "video.fullscreen": "1",
    "video.internal_scale": "2",
    "input.mouse": "0",
    "input.touch_controls": "0",
    # En la consola se confirma con A, como en los demás juegos de la Xbox (la versión en español venía de la PC
    # con «east», el botón derecho). Se cambia en el menú de Yakumo: Controls → Confirm button.
    "input.confirm": "south",
}
ENTORNO = [
    "# Variables de entorno del port UWP: NOMBRE=valor por línea (uwp_app.cpp las fija al arrancar).",
    "# MHP3RD_DATA_DIR la pone el port: es este LocalState.",
    "MHP3RD_PERF=log",
]


def escribe(ruta, texto):
    tmp = ruta.with_name(ruta.name + ".tmp")
    tmp.write_text(texto, encoding="utf-8", newline="\n")
    os.replace(tmp, ruta)


def main():
    SALIDA.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(DATOS_PC / "EBOOT.ELF", SALIDA / "EBOOT.ELF")
    lineas = (DATOS_PC / "settings.ini").read_text(encoding="utf-8").splitlines()
    vistos = set()
    salida = []
    for l in lineas:
        clave = l.split("=", 1)[0] if "=" in l and not l.startswith("#") else None
        if clave in CAMBIOS:
            salida.append(f"{clave}={CAMBIOS[clave]}")
            vistos.add(clave)
        else:
            salida.append(l)
    for clave, valor in CAMBIOS.items():
        if clave not in vistos:
            salida.append(f"{clave}={valor}")
    escribe(SALIDA / "settings.ini", "\n".join(salida) + "\n")
    escribe(SALIDA / "entorno.txt", "\n".join(ENTORNO) + "\n")
    # Los mods (la traducción de la versión en español): mods.ini y la carpeta mods\, tal cual.
    if (DATOS_PC / "mods.ini").is_file():
        shutil.copyfile(DATOS_PC / "mods.ini", SALIDA / "mods.ini")
    if (DATOS_PC / "mods").is_dir():
        shutil.copytree(DATOS_PC / "mods", SALIDA / "mods", dirs_exist_ok=True)
    for f in sorted(SALIDA.rglob("*")):
        if f.is_file():
            print(f"{f.relative_to(SALIDA)}: {f.stat().st_size} B")
    print("cambios en settings.ini:", ", ".join(f"{k}={v}" for k, v in CAMBIOS.items()))


if __name__ == "__main__":
    main()
