# MP DOOM 1.0.2

Play the original DOOM on MPC Sample from an SD card. The starter includes the unchanged, free shareware episode. The full-game setup uses your own supported GOG Ultimate DOOM installer. Both use the same prebuilt engine; no AI or compiling is needed.

This release targets MPC Sample firmware 1.3.0.12. It runs the game from RAM and returns to the stock MPC application when you quit. It does not flash firmware or install a permanent service. This public launcher does not start SSH or include access keys.

## Choose a download

- **Starter:** unzip `mp-doom-1.0.2-starter.zip`. Its `SD/autorun` folder is ready to copy on Windows and macOS.
- **Full game:** unzip `mp-doom-1.0.2-full-setup.zip`, then follow the Windows or Mac instructions below. Commercial game data is not included.
- **Matching source:** `mp-doom-1.0.2-source.zip` contains the engine source, modifications, build scripts and runtime source. It is for developers; players do not need it.

All downloads include version and license information. Compare the ZIP's hash with `SHA256SUMS.txt` using `Get-FileHash .\mp-doom-1.0.2-starter.zip` in PowerShell or `shasum -a 256 mp-doom-1.0.2-starter.zip` on macOS.

## What you need for the full game

From your GOG library, download the **offline backup installer** named `setup_the_ultimate_doom_1.9_(28044).exe`. The setup supports that exact installer, SHA-256 `d82f77b9dafc2b20fe71054473d32b5466722b9cb9b7db4a004cd61d903c2ba9`. GOG Galaxy, other installer versions and other DOOM editions are unsupported.

## Full-game setup on Windows

1. In File Explorer, right-click the full-game ZIP and choose **Extract All**. Extract it to a folder on your PC, such as Downloads.
2. Open that folder and double-click **Start MP DOOM Setup** (the `.cmd` file). Select the GOG offline installer in the file picker and click **Open**.
3. When you see **Ready**, open the new `MPC-DOOM-SD` folder next to the setup file. Follow the card steps below to copy the `autorun` folder inside it.

The Windows setup includes its extractor and runs offline without Python or administrator access. It reads the GOG installer without running it. If you run setup again, move the existing `MPC-DOOM-SD` folder aside first.

## Full-game setup on macOS

You need Python 3.9 or newer and `innoextract` 1.9 or newer on your computer. Setup was tested on Apple Silicon macOS. Install Python 3 from python.org or your package manager if `python3 --version` is unavailable. If you use Homebrew, install the extractor with:

```sh
brew install innoextract
```

Open Terminal in the unzipped full-setup folder and run:

```sh
python3 setup_full_game.py --installer "$HOME/Downloads/setup_the_ultimate_doom_1.9_(28044).exe" --output "$HOME/Desktop/MPC-DOOM-SD"
```

The tool verifies the installer, asks `innoextract` to read only `DOOM.WAD` into a temporary folder, validates the exact unchanged WAD and all 36 maps, then creates `MPC-DOOM-SD/autorun`. It never executes the Windows installer. It refuses to replace an existing output folder. Temporary extracted files are deleted after success or failure.

The engine is included. The generated folder contains your purchased game data, so keep it private. Do not upload or redistribute it.

## Install the SD folder

1. Save your MPC project and shut the MPC down normally. Use a card that already works with the stock MPC Sample. Back up the card to your computer first.
2. Open the card on your computer. If it already has an `autorun` folder, copy that entire folder to your computer, then rename the card copy to `autorun.disabled`. Do not merge the old and new folders.
3. Copy the starter's `SD/autorun`, or the full setup's generated `autorun`, to the **root of the card**. The result must be `<card>/autorun/autorun.sh`, not `<card>/SD/autorun/...`. Keep all files together. Do not change `MPC-Sample` or format the card.
4. Eject the card safely. With the MPC powered off, insert it and turn the MPC on. The game should launch automatically into episode 1, map 1 on easy difficulty. Use New Game to choose another available episode or skill.

Use only one active `autorun` folder. The public launcher and the older private development launcher are different packages. Changing between them requires replacing the whole folder while the MPC is off.

## Controls

| Control | Action |
| --- | --- |
| Big encoder | Aim; navigate menus; adjust an active slider; choose save-name characters |
| Encoder push | Fire; select; enter/finish slider adjustment; commit a save-name character |
| Pads 10 / 6 | Forward / backward |
| Pads 5 / 7 | Strafe left / right |
| Pad 4 | Fire, hold for repeated shots |
| Pad 8 | Open doors / use |
| SHIFT | Tap to toggle RUN ON / RUN OFF for this session |
| MAIN VOLUME | Overall sound and original FM music volume; full left mutes |
| PLAY | Select / confirm; enter or finish slider adjustment; save an edited name |
| SEQ | Open / close menu; cancel name editing |
| STOP | Back / cancel; finish slider adjustment; does not quit gameplay |
| ERASE | Parent menu; backspace in a save name |
| Pad 12 | Automap |
| Pads 13 / 14 / 15 / 16 / 1 / 2 / 3 | Weapons 1 / 2 / 3 / 4 / 5 / 6 / 7, when owned |
| Pads 9 / 11 | Menu slider left / right |
| PLUS / MINUS | Yes / No in prompts; cursor right / left in save names |
| Short Power press and release | Exit the game and return to the stock MPC |

Open **Read This** for the three-page MPC guide. In a slider, PLAY or push enters adjustment, turn changes the value, then PLAY/push finishes. STOP finishes without undoing your adjustment.

In a save name, turn chooses A–Z, digits, space, hyphen or period. Push commits and advances; PLUS/MINUS move the cursor; ERASE deletes; PLAY saves a nonblank name. STOP/SEQ cancels. Only committed characters are saved. A successful save resumes gameplay and shows GAME SAVED.

To exit from the menu, choose Quit Game and confirm **RETURN TO THE MPC?** with PLUS. MINUS, STOP or SEQ cancels. A short Power release exits directly. The public release has no automatic play-time limit.

## Saves, backups and settings

Six save slots persist on the card in `MPC-DOOM/saves/shareware/` or `MPC-DOOM/saves/ultimate-doom/`. The two games deliberately use different folders. Do not move a save between editions. Leave the card inserted while playing, saving and exiting.

After quitting and shutting down normally, copy the entire `MPC-DOOM/saves` folder to your computer for a backup. To restore, with the MPC off, copy the matching edition's folder back to the same location on the card. Back up existing slots before replacing them. File names are `doomsav0.dsg` through `doomsav5.dsg`.

Menu volume, screen size, sensitivity and run mode are session settings. Configuration and logs live in RAM and disappear after a restart. Saving a game does not make those settings permanent.

## Return the card to normal use

Quit to stock, shut down normally, and remove the card before editing it on your computer. Rename `autorun` to `autorun.disabled`, or delete it after backing it up. Keep `MPC-DOOM/saves` if you want to play again. Restore your previous autorun backup only if you want its earlier behavior. Stock projects and samples stay in `MPC-Sample`.

Booting with no autorun card returns to normal stock operation. Removing a card while the game is running is not an exit method because the engine runs from RAM.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Stock starts instead of DOOM | Check the root-level `autorun/autorun.sh` layout, complete folder copy, supported firmware and writable card. Recopy the original folder with the MPC off. |
| Game will not start | Confirm the card is writable and has free space for saves. Use the unmodified package; its engine, WAD and launcher are hash checked before launch. |
| Card mounts under an unusual path | The MPC's normal `/media/MPCsample` path is supported. Mount paths with spaces or special characters are rejected before save writes. |
| Full setup rejects the installer | It needs the exact named offline installer and hash above. Keep your original file; this tool does not patch or convert other versions. |
| Windows setup cannot find a file | Use **Extract All** and run setup from the extracted folder. Keep the `windows` subfolder next to the setup file. |
| macOS cannot find `innoextract` | Run `innoextract --version`, install it, or pass `--extractor /absolute/path/to/innoextract`. |
| Output already exists | On Windows, move the existing `MPC-DOOM-SD` folder aside. On Mac, choose a new `--output` folder. |
| No sound | Turn MAIN VOLUME up, then check SFX and Music Volume in Options. Check whether headphones or line-out are connected. |
| Save fails | Keep the card inserted, check free space and write protection, and back up existing saves before retrying. A failed checked write is not reported as a successful save. |
| A save is missing | Check that you launched the same edition and retained its matching `MPC-DOOM/saves` folder. |
| Need stock back | Use a short Power release or menu Quit. If needed, shut down normally, remove the card while off and restart. |

Phil confirmed that the approved opening displayed before gameplay with Ultimate DOOM. The main play tests of both editions were performed on the earlier 1.0.0 packages. See `VERIFICATION.md` for the recorded results and remaining checks.

## Licenses

The engine and local port are GPL version 2 or later. See `ENGINE-LICENSE.txt`, `MUSIC-NOTICES.txt`, `RUNTIME-NOTICES.txt` and the matching source archive. DOOM names and game content belong to their respective owners; this is an independent port.

The starter's original `doom.wad` is shareware game data, separately licensed from the engine. It is copied byte for byte from `DOOM1.WAD` in the original `doom95.zip` mirror at https://www.gamers.org/pub/idgames/idstuff/doom/win95/doom95.zip. Its SHA-256 is `1d7d43be501e67d927e415e0b8f3e29c3bf33075e859721816f652a526cac771`. The preserved id license and John Carmack distribution clarification are in `SHAREWARE-LICENSE.txt`, obtained from https://sources.debian.org/src/doom-wad-shareware/1.9.fixed-4/debian/copyright/. Distribute this starter free and unchanged; the terms do not permit charging for receipt or use without id's consent. That record is historical permission preserved by Debian, not a new license from the current rights holder.

The full setup contains no commercial WAD, installer, manual or commercial license files. Your own GOG installer and extracted game data remain subject to their purchase terms.

The Windows setup includes the original [innoextract 1.9 Windows release](https://constexpr.org/innoextract/) by Daniel Scharrer. Its documentation and license notices are in `windows/innoextract`.
