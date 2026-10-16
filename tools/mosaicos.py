"""Mosaicos del paquete del port (etapa 4): la pantalla de título del juego, recortada a cada tamaño.

Sale de docs\\imagenes\\01-pc-titulo.png, una captura de la copia del usuario; el paquete es para
su consola y no se publica. Uso: py -3.13 mosaicos.py   (los deja en package\\Assets\\)

Solo sirve mientras no haya arte del usuario. Desde el 2026-09-28 lo hay en dependencias-usuario\\Display
Images\\, y los mosaicos los pone Recompilaciones\\herramientas\\identidad-visual.py; entonces este script se niega.
"""
import os
import pathlib

from PIL import Image

AQUI = pathlib.Path(__file__).resolve().parent
ARTE = AQUI.parent.parent / "dependencias-usuario" / "Display Images"
if any(ARTE.glob("xbox-*.png")):
    raise SystemExit("hay arte del usuario en dependencias-usuario\\Display Images: los mosaicos los pone "
                     "identidad-visual.py --xbox \"Monster Hunter Portable 3rd\" --zona IA")
ORIGEN = AQUI.parent.parent / "docs" / "imagenes" / "01-pc-titulo.png"
SALIDA = AQUI / "package" / "Assets"
TAMANOS = {"StoreLogo.png": (50, 50), "Square44x44Logo.png": (44, 44), "Square150x150Logo.png": (150, 150),
           "Wide310x150Logo.png": (310, 150), "SplashScreen.png": (620, 300)}

titulo = Image.open(ORIGEN).convert("RGB")
SALIDA.mkdir(parents=True, exist_ok=True)
for nombre, (w, h) in TAMANOS.items():
    # Cubrir el tamaño sin deformar: escalar por el lado que falta y recortar al centro.
    escala = max(w / titulo.width, h / titulo.height)
    im = titulo.resize((round(titulo.width * escala), round(titulo.height * escala)), Image.LANCZOS)
    x0 = (im.width - w) // 2
    y0 = (im.height - h) // 2
    im = im.crop((x0, y0, x0 + w, y0 + h))
    tmp = SALIDA / (nombre + ".tmp")
    im.save(tmp, format="PNG")
    os.replace(tmp, SALIDA / nombre)
print("mosaicos en", SALIDA)
