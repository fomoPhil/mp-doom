# MP DOOM 1.0.2 verification

Phil confirmed that the approved opening displayed before gameplay with Ultimate DOOM. The 1.0.2 downloads use the same engine as 1.0.1. The opening duration, help credit and Power exit during the opening were checked on the computer.

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

- Observe the opening on the starter and check its duration, help credit and Power exit on the MPC.
- Confirm USB-disconnected boot for both editions and test menu Quit and short Power separately, including stock pad sound and controls.
- Create a new full-game save in an unused slot and load it after a power cycle.
- Check individual controls, volume and Read This, play past 30 minutes, verify headphone and line-out routing, and confirm stock boot with the card removed.

## Archive integrity

Compare download hashes against `SHA256SUMS.txt`. `ARCHIVE-AUDIT.json` records each archive and included file's size and hash. The builder uses fixed timestamps, sorted entries and fixed file modes, and checks the build record and generated sources against the release engine.
