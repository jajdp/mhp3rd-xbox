# How it works, and why each piece is there

*(Español: [HOW-IT-WORKS.es.md](HOW-IT-WORKS.es.md))*

## The underlying problem: an Xbox has no Vulkan

Yakumo draws **with Vulkan only**. A UWP app on an Xbox has Direct3D 12 and nothing else: there is
no Vulkan driver, and no way to install one.

The way out is **Dozen**, Mesa's layer that implements Vulkan on top of D3D12. And that changes the
shape of the work: it is not one port, it is two layers that have to work at the same time — the
game on Vulkan, and Vulkan on D3D12 inside a container.

Which is why the work was done **in four stages, in this order**, and that is the most reusable part
of all of it:

| | |
|---|---|
| 1 | The game on the **PC** with Dozen, not with the vendor's Vulkan. If it does not work there, nothing else matters |
| 2 | A **minimal app** with the UWP Dozen, installed on the console. It creates the instance, picks a device, draws, and reports what happened. That is this repository's `src/dozen/` |
| 3 | Build Yakumo **from source**, rather than using the release |
| 4 | The UWP port itself |

Skipping stage 2 is what turns a port into a week of guessing: when the game does not draw, there is
no way to tell whether it is the game, the packaging or the layer.

## The black screen, and a hypothesis of mine that was wrong

With the port installed, the game **was running** — there was sound, it was progressing — and the
screen was **black**.

The first hypothesis was mine and it was **wrong**: that a Store app has a video memory cap and we
were over it. It cost an afternoon and that was not it.

What found it was to stop assuming and **query the formats one by one**. The game asked for
`D32_SFLOAT` for its depth buffer, and on that layer the format reported **0 bytes per pixel**:
there was no depth, so everything was being discarded. With **`D24S8`** it draws.

The lesson, which holds for any port on top of a translation layer: **what a format claims to
support and what it actually reports can differ**, and asking costs ten lines.

## Music and movies: FFmpeg for UWP

The game wants them, and FFmpeg **is not published built for UWP**. It has to be built: with MSYS2,
configured against the "store" CRT and without the parts an App Container does not allow.

That is `tools/ffmpeg_uwp.sh` (what runs inside MSYS2) and `tools/ffmpeg_uwp.ps1` (what calls it
from Windows). **It is the most reusable piece in this repository**: any other UWP port that needs
FFmpeg can use it as is, changing only which libraries it wants.

## The ISO, in parts

The Device Portal will not take the ISO in one go. It goes in 90 MiB parts
(`tools/partir_iso.py`), and the game's data is prepared separately
(`tools/datos_consola.py`) because the port reads it from `LocalState`.

That has a happy consequence: **a translated version needs no second package**. The translation mods
are uploaded into `LocalState` and the game reads them; the `.msix` is the same one.

## What is missing

- **Developer Mode suspension**: coming back from Home, the game starts over.
- A translated build installs by uploading its mods into `LocalState`, but it is worth having as a
  sibling package with its own identity so that saves do not mix.
