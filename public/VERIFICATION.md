# MP DOOM 1.0.4 verification

## 1.0.4 card name checks

1.0.4 changes the launcher scripts, so DOOM starts from an exFAT card with any name, and raises the Main Volume ceiling from 40% to 50%. `doom/mpc_sound.c` is the only engine source that changed.

The launcher ran on real exFAT mounts under real systemd and BusyBox with a stand-in game and 15 card names, including spaces, an apostrophe, an ampersand, an accent and symbols. Each one started, wrote a save to the card, stayed inside the save sandbox and returned to stock. Non-exFAT and read-only cards are refused before anything is written.

With real shareware sounds at maximum Main Volume and SFX volume, a single loud centered sound peaks at -12.04 dBFS, up from -13.98. Eight of the loudest sounds together peaked at -0.6 dBFS without clipping. Adding half-scale stand-in music clipped 24 of about 176,000 samples in the worst of 24 alignments.

Phil booted the final 1.0.4 starter from an exFAT card named `MP DOOM`, a name 1.0.3 refused. Phil reported that it reached DOOM, the louder Main Volume was good, saving worked and a short Power press returned to the stock screen.

## 1.0.3 title card checks

1.0.3 changes only the opening title card. Rebuilding with the 1.0.2 artwork produced the 1.0.2 engine byte for byte, so no other engine code changed. The new engine passed the opening test (normal start, Power and signal exit during the opening, and a display flush failure) and the release unit tests.

The new engine and its launcher files were copied to Phil's Ultimate DOOM MPC Sample card and verified on readback. Only `mpc-doom`, `autorun.sh` and `autorun.check` changed; the game data and all five saves were untouched. Phil restarted the MPC and confirmed that the new title card displayed before DOOM.

The Windows and Mac setup are unchanged from 1.0.2. The checks below were recorded on 1.0.2 and earlier releases.

## 1.0.2 Windows setup checks

On Windows 11 Pro (x64), the packaged launcher processed the supported GOG offline installer without running it. The eight files it generated matched the Mac setup output byte for byte. Paths with spaces, brackets and Unicode worked. Setup rejected an unsupported installer, a modified extractor and an existing output folder. Temporary setup files were removed.

The starter's eight SD files matched the prior release. Windows copied both editions to a physical exFAT MPC Sample SD card, and all eight files for each edition matched on readback. Ultimate DOOM was installed as the active launcher. The previous launcher was backed up and all five existing saves were unchanged.

Phil booted the MPC Sample from the Windows-prepared Ultimate DOOM card and confirmed that it reached gameplay, loaded existing saves, and played sound normally.

For the hands-on Windows setup check, the extracted full-setup ZIP produced a new `MPC-DOOM-SD` folder. All eight files matched the Ultimate DOOM payload that booted on the MPC.

## 1.0.1 computer checks

The generated opening matched the approved artwork pixel for pixel. Both editions passed engine tests for controls, menus, help, music, save and reload, and quit. A clean build from the matching source produced the same engine bytes. The source ZIP includes the opening artwork generator.

## Earlier 1.0.0 hardware results

Both 1.0.0 public packages passed the main MPC Sample play test. Each booted, played sound, loaded a save and returned to the stock screen. The starter also passed creating a save and loading it after a power cycle. Phil reported that controls and sound worked on the full game, but individual controls were not separately recorded.

## Remaining checks

- Observe the 1.0.4 opening on the starter and check its duration and help credit on the MPC.
- Confirm USB-disconnected boot for both editions and test menu Quit and short Power separately, including stock pad sound and controls.
- Create a new full-game save in an unused slot and load it after a power cycle.
- Check individual controls, volume and Read This, play past 30 minutes, verify headphone and line-out routing, and confirm stock boot with the card removed.

## Archive integrity

Compare download hashes against `SHA256SUMS.txt`. `ARCHIVE-AUDIT.json` records each archive and included file's size and hash. The builder uses fixed timestamps, sorted entries and fixed file modes, and checks the build record and generated sources against the release engine.
