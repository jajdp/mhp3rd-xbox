"""Parte la imagen del disco en trozos que el Device Portal sí acepta.

    py -3.13 partir_iso.py [imagen] [carpeta de salida] [MiB por parte]
    (el ISO, la carpeta de salida y los MiB por trozo se le pasan por argumento; 90 por omisión)

El portal de la consola rechaza con un error 500 las subidas de más de unos cientos de MB (el
2026-09-28, 100 MB subieron y 500 MB no). Deja disc.iso.001, disc.iso.002… y disc.iso.partes, con el
número de partes y el tamaño total; uwp_app.cpp las junta en LocalState al arrancar y las borra
cuando disc.iso tiene exactamente ese tamaño.
"""
import hashlib
import os
import pathlib
import sys

IMAGEN = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else r"<MHP3RD_ARBOL>\profiles\mhp3rd\game\disc.iso")
SALIDA = pathlib.Path(sys.argv[2] if len(sys.argv) > 2 else r"<MHP3RD_PKG>\partes")
MIB = int(sys.argv[3]) if len(sys.argv) > 3 else 90


def main():
    SALIDA.mkdir(parents=True, exist_ok=True)
    total = IMAGEN.stat().st_size
    tam = MIB << 20
    huella = hashlib.sha256()
    n = 0
    with open(IMAGEN, "rb") as f:
        while True:
            datos = f.read(tam)
            if not datos:
                break
            n += 1
            huella.update(datos)
            destino = SALIDA / f"disc.iso.{n:03d}"
            tmp = destino.with_name(destino.name + ".tmp")
            with open(tmp, "wb") as g:
                g.write(datos)
            os.replace(tmp, destino)
    lista = SALIDA / "disc.iso.partes"
    tmp = lista.with_name(lista.name + ".tmp")
    tmp.write_text(f"{n} {total}\n", encoding="ascii", newline="\n")
    os.replace(tmp, lista)
    print(f"{n} partes de {MIB} MiB, {total} B, SHA-256 {huella.hexdigest()}")


if __name__ == "__main__":
    main()
