# Monster Hunter Portable 3rd HD on Xbox — the UWP port

How to take the **Yakumo** ([TeamGDB](https://github.com/TeamGDB/Yakumo)) static recompilation of
*Monster Hunter Portable 3rd HD* (PSP, 2010) and run it **on an Xbox Series in Developer Mode**, as a
UWP package you build and sign yourself.

*(Español: [README.es.md](README.es.md))*

It is not a theory: the game boots, plays with a controller, **and has sound** — music and movies
included.

## What makes this port special: an Xbox has no Vulkan

Yakumo draws **with Vulkan only**. And a UWP app on an Xbox has no Vulkan: it has Direct3D 12 and
nothing else.

The way out is **Dozen**, Mesa's layer that implements Vulkan on top of D3D12. That turns the port
into a two-layer problem instead of one: both the game **and** the Vulkan it believes it has must be
brought to UWP. Which is why this repository also carries **the minimal app that proved Dozen worked
on the console before the game was attempted** (`src/dozen/`): a 45 KB test that draws a triangle and
says whether the layer is alive. If Dozen does not come up there, there is no port to attempt.

## What this repository is

| Piece | What it is |
|---|---|
| `tools/parchar_uwp.py` | The port's changes to Yakumo's tree, with checked anchors |
| `tools/construir-uwp.ps1` | The build script: compiles, lays out the package and signs |
| `tools/ffmpeg_uwp.ps1` and `.sh` | **FFmpeg built for UWP**, which is what gives music and movies. It is the most reusable piece here: any UWP port that needs FFmpeg can use it as is |
| `tools/compilar-pc.ps1` | Building Yakumo from source, which is the step before |
| `tools/partir_iso.py`, `datos_consola.py` | The ISO in parts, and the game's data for the console |
| `src/dozen/` | The minimal app that tests Dozen on the console |

## What it is not

There is no **ISO, no game data, no executable, no `.msix`, no tiles, no Mesa and no built FFmpeg**
here. You need your own copy of the game and Yakumo building first. See [`NOTICE.md`](NOTICE.md).

## What you need first

| | |
|---|---|
| **Yakumo** | [TeamGDB/Yakumo](https://github.com/TeamGDB/Yakumo); this was done with `v0.6.0-alpha.4`, and it has to be **building on your PC first** |
| **Your own copy** of the game | An ISO of *Monster Hunter Portable 3rd HD* |
| **Mesa with Dozen**, for desktop and for UWP | The Vulkan-on-D3D12 layer. Without it there is no drawing |
| **MSYS2** | To build FFmpeg for UWP (`tools/ffmpeg_uwp.sh`) |
| **Visual Studio 2022** with the **C++ UWP component** | Building and packaging |
| **SDL3** for Visual C | Window and input |
| **Python 3**, an Xbox in Developer Mode and a certificate | The scripts, the console and signing |

## How it goes

```powershell
# the paths, once per session (docs/INSTALL.md §2)
$env:MHP3RD_ARBOL = "C:\mh\yakumo"; $env:MHP3RD_PKG = "C:\mh\package"
$env:MHP3RD_CERT  = "<your certificate's thumbprint>"

python tools\parchar_uwp.py                    # the port's changes
powershell -ExecutionPolicy Bypass -File tools\construir-uwp.ps1
```

Step by step, including the console side: **[`docs/INSTALL.md`](docs/INSTALL.md)**.

## What it had to solve

The detail is in **[`docs/HOW-IT-WORKS.md`](docs/HOW-IT-WORKS.md)**.

| | Symptom | Cause |
|---|---|---|
| 1 | No Vulkan on the console | **Dozen**, Mesa's layer on D3D12. And, before attempting the game, the minimal app in `src/dozen/` that proves it is alive |
| 2 | **A black screen**, with the game running behind it | The depth format: `D32_SFLOAT` reported **0 bytes** per pixel on that layer. With `D24S8` it draws. ⚠ The first hypothesis — that it was a Store app's memory cap — was **mine and it was wrong**, and it cost an afternoon: what found it was querying the formats one by one |
| 3 | No music and no movies | FFmpeg did not come for UWP: it is built with MSYS2 against the "store" CRT (`tools/ffmpeg_uwp.sh`) |
| 4 | The ISO will not upload through the Device Portal | In 90 MiB parts, which the game joins |

## Credits and license

Port and tooling by **Recompilaciones**, under the [MIT](LICENSE), the same as
[Yakumo](https://github.com/TeamGDB/Yakumo), which is the code this patches.

*Monster Hunter Portable 3rd* is © Capcom. This project is not affiliated with them, with Microsoft,
or with TeamGDB, and distributes nothing that belongs to them. Rights holders:
**jajdpmail@gmail.com**.
