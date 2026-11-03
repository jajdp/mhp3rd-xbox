# Changelog

All notable changes to this repository. Dates are `YYYY-MM-DD`.

## [1.0.3] — 2026-11-03

### Changed

- **This repository no longer explains itself by pointing at other repositories of this
  catalogue.** Comments and documentation introduced a decision, a trap or a default as "the same
  as in such-and-such other port of ours" — which asked anyone who came here for this game to go
  and read about a different one first. Each of those comparisons carried a fact that reads better
  stated outright, so that is what is written now.

  What is still cited is everything that is **not** ours: the upstream project, the third-party
  forks the recipe is built on, and the files taken from them under their own license.

## [1.0.2] — 2026-10-20

### Changed

- **The README now says which console this was tested on, and which one it was not.** Everything here
  was built and measured on an **Xbox Series X** in Developer Mode; a **Series S has never been tried**,
  so nothing here says anything about one. The wording used to say "Xbox Series", which reads as if it
  covered both, and it does not.

## [1.0.1] — 2026-10-16

### Fixed

- **`LICENSE`: the explanatory block named the wrong project.** Everything after the MIT text had
  been copied from another repository in this catalogue and never adapted: it said this was a
  derivative of a different project altogether, denied granting rights in a game that is not this
  one, quoted "the 57 lines of the upstream project", and listed folders this repository does not
  have. The MIT grant itself was correct all along, and `NOTICE.md` always had the right figures.
  The block now names TeamGDB/Yakumo (MIT) and Monster Hunter Portable 3rd, and points at
  `NOTICE.md` for the measurements instead of repeating a number.

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
