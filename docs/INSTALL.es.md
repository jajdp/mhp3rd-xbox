# Construir el port y meterlo en la consola

*(English: [INSTALL.md](INSTALL.md))*

## 0. Dónde se escriben las órdenes

En **PowerShell**, la terminal normal de Windows: tecla **Windows**, escribe `powershell`, **Enter**
sobre *Windows PowerShell*. Para ir a una carpeta, `cd` con la ruta entre comillas si tiene
espacios.

**No hace falta administrador** en ningún paso. Y en el del certificado, usarlo lo **estropea**: el
certificado se crea en el almacén del usuario que ejecuta la orden, así que elevado acabaría en el
del administrador y la firma no lo encontraría.

**PowerShell 7 no hace falta**, y los ejemplos llevan `-ExecutionPolicy Bypass` porque un Windows de
fábrica no ejecuta un `.ps1`.

---

## 1. Lo que hace falta antes

| | Para qué |
|---|---|
| **Yakumo** compilando en tu PC | La recompilación del juego. Esto se hizo con la `v0.6.0-alpha.4` |
| **Tu propia copia** del juego, en ISO | Yakumo prepara sus datos a partir de ahí |
| **Mesa con Dozen**, de escritorio y **para UWP** | La capa Vulkan-sobre-D3D12. Sin ella no hay dibujo en la consola |
| **MSYS2** | Compilar FFmpeg para UWP |
| **Visual Studio 2022** con el componente de **UWP para C++** | Compilar y empaquetar |
| **SDL3** para Visual C | Ventana y entrada |
| **Python 3** | Los parches y los scripts |
| **Una Xbox en modo desarrollador** y un certificado | Instalar y firmar |

---

## 2. Las variables de entorno

Los scripts no llevan ninguna ruta de ninguna máquina dentro: se ponen una vez por sesión de
terminal.

```powershell
$env:MHP3RD_ARBOL  = "C:\mh\yakumo"        # el arbol de Yakumo, ya compilado para PC
$env:MHP3RD_PKG    = "C:\mh\paquete"       # donde se monta el paquete
$env:MHP3RD_PY     = "C:\mh\yakumo-py"     # el entorno de Python de Yakumo
$env:MHP3RD_SDL3   = "C:\mh\SDL3"          # SDL3 para Visual C
$env:MHP3RD_FFMPEG = "C:\mh\ffmpeg-uwp"    # el arbol de FFmpeg para UWP
$env:MHP3RD_MSYS   = "C:\msys64"            # el MSYS2 que lo compila
$env:MHP3RD_CERT   = "<la huella de tu certificado>"
```

---

## 3. Antes del juego: comprobar que Dozen arranca en la consola

Este paso no es opcional, y es lo que ahorra la tarde: **una Xbox no tiene Vulkan**, y Yakumo dibuja
solo con Vulkan. Si la capa Dozen no arranca dentro de un App Container, no hay port que intentar.

`src/dozen/` es una aplicación mínima que hace exactamente esa pregunta: crea una instancia de
Vulkan, elige dispositivo, dibuja y dice qué pasó. Compílala, empaquétala, instálala en la consola y
mira su registro **antes** de tocar el juego.

---

## 4. Yakumo, compilado para PC

```powershell
powershell -ExecutionPolicy Bypass -File tools\compilar-pc.ps1
```

Ese árbol compilando es el punto de partida del port: los parches se aplican sobre él.

⚠ Pruébalo en el PC —con el Dozen de escritorio— antes de seguir. Si no corre ahí, no va a correr en
la consola.

---

## 5. Los parches, y el certificado

```powershell
python tools\parchar_uwp.py
```

Cada cambio comprueba su ancla antes de escribir: si una no cuadra, te lo dice en vez de dejar el
árbol a medias.

El certificado, si no tienes uno:

```powershell
$c = New-SelfSignedCertificate -Type Custom -Subject "CN=TuNombre" `
       -KeyUsage DigitalSignature -FriendlyName "Firma UWP" `
       -CertStoreLocation "Cert:\CurrentUser\My" `
       -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")
$c.Thumbprint
```

Y en `package\AppxManifest.xml`, los `CHANGE-ME`: `Identity Name`, `Publisher` (**idéntico** al
sujeto de tu certificado), `PublisherDisplayName` y el `PhoneProductId`.

---

## 6. FFmpeg para UWP: la música y las películas

Sin esto el juego corre, pero **mudo y sin vídeos**. FFmpeg no se publica compilado para UWP, así
que se compila:

```powershell
powershell -ExecutionPolicy Bypass -File tools\ffmpeg_uwp.ps1
```

Ese guion llama a `tools/ffmpeg_uwp.sh` dentro de MSYS2, que es quien configura FFmpeg contra el CRT
«store» y lo compila. Es la pieza más reutilizable de este repositorio: **cualquier port UWP que
necesite FFmpeg puede usarla igual**.

---

## 7. Los mosaicos, compilar y empaquetar

`package\Assets\` viene **vacía a propósito** ([`ASSETS.es.md`](ASSETS.es.md));
`tools/mosaicos.py` las genera de una imagen tuya.

```powershell
powershell -ExecutionPolicy Bypass -File tools\construir-uwp.ps1
```

⚠ **La versión sube en cada paquete nuevo**: la consola no reemplaza un paquete por otro de la misma
versión, ni cambia sus mosaicos.

---

## 8. Instalarlo en la consola

### Antes de tocar nada: cómo tiene que estar la consola

- **En modo desarrollador**, con Dev Home instalado. - **Encendida y con tu perfil iniciado.**
  ⚠ Sin un usuario conectado, la consola **no abre aplicaciones**: el paquete se instala,
  aparece en la lista y al lanzarlo no pasa nada. - **En la misma red**, con su dirección a
  mano. El Device Portal escucha en el puerto **11443** por HTTPS, y pide el usuario y la
  contraseña que definiste en Dev Home.

Abre `https://<ip>:11443`. ⚠ El navegador avisará de que el certificado del sitio no es de
confianza: es el de la propia consola. Entra igual — en Edge y Chrome, **Avanzado** → **Continuar
a … (no seguro)**; en Firefox, **Avanzado** → **Aceptar el riesgo y continuar**. Luego, en
**My games & apps → Add**, tu `.msix`.

---

## 9. El juego, dentro del paquete

Los datos del juego van al `LocalState` de la aplicación, que crea el primer arranque. El ISO no
sube de una vez por el Device Portal, así que:

```powershell
python tools\partir_iso.py <tu.iso> <carpeta de salida> 90
python tools\datos_consola.py <datos de Yakumo> <carpeta para la consola>
```

Sube los trozos y los datos con el **File explorer** del portal. El port lee los datos de
`LocalState`, que es también por donde entra la **versión en español**: los mods de traducción se
suben ahí, sin tocar el paquete.

---

## Si algo no sale bien

| Síntoma | Causa |
|---|---|
| El paquete instala y al abrirlo no pasa nada | ¿Hay un **perfil iniciado**? Y mira el registro del `LocalState` |
| **Pantalla en negro**, pero el juego se oye o avanza | El formato de profundidad: `D32_SFLOAT` devuelve 0 bytes por píxel en esa capa. Tiene que ser `D24S8` ([`HOW-IT-WORKS.es.md`](HOW-IT-WORKS.es.md)) |
| No arranca el dibujo | Dozen no está, o no es el que funciona. Comprueba primero la aplicación mínima del §3 |
| Mudo y sin películas | Falta el FFmpeg para UWP (§6) |
| El portal no acepta el ISO | En trozos de 90 MiB (§9) |
