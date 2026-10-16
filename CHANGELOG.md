# Changelog

All notable changes to this repository. Dates are `YYYY-MM-DD`.

## [1.0.1] — 2026-10-16

### Fixed

- **`LICENSE`: the explanatory block named the wrong project.** Everything after the MIT text had
  been copied from the `dkr-xbox` repository and never adapted: it said this was a derivative of
  Golden Balloon, denied granting rights in Diddy Kong Racing, quoted "the 57 lines of the upstream
  project", and listed folders this repository does not have. The MIT grant itself was correct all
  along, and `NOTICE.md` always had the right figures. The block now names TeamGDB/Yakumo (MIT) and
  Monster Hunter Portable 3rd, and points at `NOTICE.md` for the measurements instead of repeating a number.

## [1.0.0] — 2026-10-13

First publication. The recipe for the UWP port, as it was applied to the build that plays on a real
Xbox Series X, with music and movies.

### Added

- **`tools/parchar_uwp.py`** — the port's changes to Yakumo's tree, each one checking its anchor
  before writing.
- **`tools/construir-uwp.ps1`** — the build: compile, lay out the package, sign.
- **`tools/ffmpeg_uwp.ps1` and `.sh`** — FFmpeg built for UWP, which is what gives the game its
  music and movies. The most reusable piece here.
- **`tools/compilar-pc.ps1`** — building Yakumo from source first, which is where the console build
  starts.
- **`tools/partir_iso.py`**, **`tools/datos_consola.py`** — the ISO in 90 MiB parts, and the game's
  data laid out for the console.
- **`src/dozen/`** — the minimal Vulkan app that answered the only question that mattered before
  anything else: does Mesa's Dozen layer come up inside an App Container at all? An Xbox has no
  Vulkan, and Yakumo draws with nothing else.
- **Documentation in both languages**, including the black screen that was **not** a memory cap —
  that hypothesis was wrong and cost an afternoon — but a depth format reporting 0 bytes per pixel.

### Not included, on purpose

- The ISO, the game data, the executable, the `.msix`, the saves and the package tiles.
- Mesa, Dozen, SDL3 and the built FFmpeg: other people's binaries. The recipes to get them are here.
