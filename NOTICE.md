# Notice

*(Español: [NOTICE.es.md](NOTICE.es.md))*

## Not affiliated

This is an unofficial, noncommercial fan project. It is **not affiliated with, endorsed by or
connected to Capcom, Sony or Microsoft**, nor to [TeamGDB](https://github.com/TeamGDB/Yakumo), the
authors of Yakumo, nor to the Mesa project. *Monster Hunter Portable 3rd*, *Xbox* and all related
names and marks are the property of their respective owners.

Nothing here is for sale, and nothing here is monetized.

## What this repository does not distribute

No ISO. No game data, assets, music or movies. No built executable. No `.msix`. No save data. No
package tiles. And no third-party binaries: not Mesa, not Dozen, not SDL3, and not the FFmpeg this
builds — the script that builds it is here, the result is not.

## What it does contain of other people's work

**Patches against Yakumo**, which is **MIT**. The patch script quotes the lines of its source that
each change checks before writing, so that it fails safely instead of corrupting a tree it does not
recognise. MIT permits this, and this repository is released under
[that same license](LICENSE).

**`src/dozen/`** is first-party: a minimal Vulkan application written to find out whether Mesa's
Dozen layer comes up inside an App Container, before spending time on the game itself.

**The FFmpeg build script** configures and builds FFmpeg from its own sources, against the UWP CRT.
FFmpeg is LGPL/GPL depending on how it is configured; what is here is a build recipe, and the
resulting binaries are yours and carry FFmpeg's own terms.

**There are no screenshots here.**

## Takedown and contact

If you hold rights in this material and want something here removed or changed, write to
**jajdpmail@gmail.com**. Requests from rights holders are honoured without argument.
