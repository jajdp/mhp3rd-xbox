# Building the port and getting it onto the console

*(Español: [INSTALL.es.md](INSTALL.es.md))*

## 0. Where the commands go

In **PowerShell**, the ordinary Windows terminal: **Windows** key, type `powershell`, **Enter** on
*Windows PowerShell*. To move to a folder use `cd` with the path, in quotes if it has spaces.

**No administrator is needed** anywhere. And in the certificate step, using it **breaks** things:
the certificate is created in the store of whoever runs the command, so elevated it would land in
the administrator's store and signing would not find it.

**PowerShell 7 is not needed**, and the examples carry `-ExecutionPolicy Bypass` because a stock
Windows will not run a `.ps1`.

---

## 1. What you need first

| | What for |
|---|---|
| **Yakumo** building on your PC | The game's recompilation. This was done with `v0.6.0-alpha.4` |
| **Your own copy** of the game, as an ISO | Yakumo prepares its data from it |
| **Mesa with Dozen**, desktop and **UWP** | The Vulkan-on-D3D12 layer. Without it there is no drawing on the console |
| **MSYS2** | To build FFmpeg for UWP |
| **Visual Studio 2022** with the **C++ UWP component** | Building and packaging |
| **SDL3** for Visual C | Window and input |
| **Python 3** | The patches and the scripts |
| **An Xbox in Developer Mode** and a certificate | Installing and signing |

---

## 2. The environment variables

The scripts carry no machine's paths inside them: set them once per terminal session.

```powershell
$env:MHP3RD_ARBOL  = "C:\mh\yakumo"        # Yakumo's tree, already built for the PC
$env:MHP3RD_PKG    = "C:\mh\package"       # where the package is laid out
$env:MHP3RD_PY     = "C:\mh\yakumo-py"     # Yakumo's Python environment
$env:MHP3RD_SDL3   = "C:\mh\SDL3"          # SDL3 for Visual C
$env:MHP3RD_FFMPEG = "C:\mh\ffmpeg-uwp"    # FFmpeg-for-UWP tree
$env:MHP3RD_MSYS   = "C:\msys64"            # the MSYS2 that builds it
$env:MHP3RD_CERT   = "<your certificate's thumbprint>"
```

---

## 3. Before the game: prove Dozen comes up on the console

This step is not optional, and it is what saves the afternoon: **an Xbox has no Vulkan**, and Yakumo
draws with nothing else. If Mesa's Dozen layer does not come up inside an App Container, there is no
port to attempt.

`src/dozen/` is a minimal app that asks exactly that: it creates a Vulkan instance, picks a device,
draws, and reports what happened. Build it, package it, install it on the console and read its log
**before** touching the game.

---

## 4. Yakumo, built for the PC

```powershell
powershell -ExecutionPolicy Bypass -File tools\compilar-pc.ps1
```

That tree building is the port's starting point: the patches are applied on top of it.

⚠ Test it on the PC — with desktop Dozen — before going on. If it does not run there, it will not
run on the console.

---

## 5. The patches, and the certificate

```powershell
python tools\parchar_uwp.py
```

Every change checks its anchor before writing: if one does not match, it tells you instead of
leaving the tree half done.

The certificate, if you do not have one:

```powershell
$c = New-SelfSignedCertificate -Type Custom -Subject "CN=YourName" `
       -KeyUsage DigitalSignature -FriendlyName "UWP signing" `
       -CertStoreLocation "Cert:\CurrentUser\My" `
       -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")
$c.Thumbprint
```

And in `package\AppxManifest.xml`, the `CHANGE-ME`s: `Identity Name`, `Publisher` (**identical** to
your certificate's subject), `PublisherDisplayName` and `PhoneProductId`.

---

## 6. FFmpeg for UWP: the music and the movies

Without this the game runs, but **silent and with no videos**. FFmpeg is not published built for
UWP, so it gets built:

```powershell
powershell -ExecutionPolicy Bypass -File tools\ffmpeg_uwp.ps1
```

That script calls `tools/ffmpeg_uwp.sh` inside MSYS2, which is what configures FFmpeg against the
"store" CRT and builds it. It is the most reusable piece in this repository: **any UWP port that
needs FFmpeg can use it as is**.

---

## 7. Tiles, build and package

`package\Assets\` ships **empty on purpose** ([`ASSETS.md`](ASSETS.md)); `tools/mosaicos.py`
generates them from an image of your own.

```powershell
powershell -ExecutionPolicy Bypass -File tools\construir-uwp.ps1
```

⚠ **Bump the version for every new package**: the console will not replace a package whose version
matches the installed one, nor change its tiles.

---

## 8. Installing it on the console

### Before touching anything: how the console has to be

- **In Developer Mode**, with Dev Home installed. - **On, and with your profile signed in.** ⚠
  With no user signed in the console **will not launch apps**: the package installs, shows up
  in the list, and launching it does nothing. - **On the same network**, with its address at
  hand. The Device Portal listens on port **11443** over HTTPS, and asks for the username and
  password you set in Dev Home.

Open `https://<ip>:11443`. ⚠ The browser will warn that the site's certificate is not trusted: it is
the console's own. Go in anyway — in Edge and Chrome, **Advanced** → **Continue to … (unsafe)**; in
Firefox, **Advanced** → **Accept the Risk and Continue**. Then, under **My games & apps → Add**,
your `.msix`.

---

## 9. The game, inside the package

The game's data goes into the app's `LocalState`, which the first launch creates. The ISO will not
upload in one go through the Device Portal, so:

```powershell
python tools\partir_iso.py <your.iso> <output folder> 90
python tools\datos_consola.py <Yakumo's data> <folder for the console>
```

Upload the parts and the data with the portal's **File explorer**. The port reads its data from
`LocalState`, which is also how a **translated version** gets in: the translation mods are uploaded
there, without touching the package.

---

## If something goes wrong

| Symptom | Cause |
|---|---|
| The package installs and launching does nothing | Is a **profile signed in**? And read the log in `LocalState` |
| **Black screen**, but the game has sound or is progressing | The depth format: `D32_SFLOAT` reports 0 bytes per pixel on that layer. It has to be `D24S8` ([`HOW-IT-WORKS.md`](HOW-IT-WORKS.md)) |
| Nothing draws at all | Dozen is missing, or is not the one that works. Check the minimal app in §3 first |
| Silent, with no movies | The UWP FFmpeg is missing (§6) |
| The portal will not take the ISO | In 90 MiB parts (§9) |
