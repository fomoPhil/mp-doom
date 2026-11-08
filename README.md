# MP DOOM

Play classic DOOM on your Akai MPC Sample. It boots from an SD card, runs from memory, and quits back to the stock MPC when you're done. Nothing is flashed or installed.

By [Phil Woolley](https://philwoolley.com/mp-doom/).

## Download

Get the latest ZIPs from [Releases](https://github.com/fomoPhil/mp-doom/releases/latest) or [philwoolley.com/mp-doom](https://philwoolley.com/mp-doom/).

| Download | What it is |
| --- | --- |
| `mp-doom-1.0.2-starter.zip` | Free. The original shareware episode, ready to copy to your card. |
| `mp-doom-1.0.2-full-setup.zip` | All four episodes of Ultimate DOOM, built from your own GOG offline installer. Windows and Mac. |
| `mp-doom-1.0.2-source.zip` | Matching source for developers. Players don't need it. |

Tested on MPC Sample firmware 1.3.0.12.

Want sounds to go with it? Grab the free [MP DOOM Kit](https://philwoolley.com/mp-doom/#pad-kit): 16 one-shots for your MPC.

## Install the starter

1. Save your project, shut the MPC down and back up your SD card.
2. Unzip the starter. Copy its `SD/autorun` folder to the top level of the card, so you end up with `<card>/autorun/autorun.sh`. If the card already has an `autorun` folder, move it aside first.
3. Eject the card, insert it with the MPC off, and turn it on. DOOM starts on its own.

For Ultimate DOOM, the full setup includes a double-click Windows setup and a Mac script. Both read your GOG installer without running it. The [player guide](public/README.md) covers both, plus saves and troubleshooting.

## Controls

| Control | Action |
| --- | --- |
| Pads 10 / 6 | Forward / back |
| Pads 5 / 7 | Strafe left / right |
| Big knob | Aim, and scroll menus |
| Knob push or pad 4 | Fire |
| Pad 8 | Open doors |
| Pad 12 | Map |
| Pads 13–16, 1–3 | Weapons 1–7 |
| SHIFT | Run on / off |
| SEQ | Menu |
| Short Power press | Quit back to the MPC |

The full control list is in the [player guide](public/README.md#controls) and in **Read This** inside the game.

## Saves

Six save slots live on the card in `MPC-DOOM/saves/`. They survive a power cycle. The starter and Ultimate DOOM keep separate folders.

## Build from source

The engine is [doomgeneric](https://github.com/ozkl/doomgeneric) with an MPC Sample port in `doom/` and FM music from Chocolate Doom's OPL code. It cross-compiles to a static ARM64 Linux binary with Zig 0.15.2. See [SOURCE-README.md](SOURCE-README.md) for the exact toolchain and build steps.

## License

The engine and the port are GPL version 2 or later. See [LICENSE](LICENSE) and [THIRD-PARTY-NOTICES.txt](THIRD-PARTY-NOTICES.txt). The starter's shareware `doom.wad` keeps its own license, included in the ZIP. No commercial game data is included anywhere.

MP DOOM is an independent fan port. It is not affiliated with id Software, Bethesda, Microsoft or Akai Professional.
