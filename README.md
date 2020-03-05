# déjàvu: Vita SaveState Plugin

*déjàvu* is a plugin for the PlayStation Vita that allows you to make a snapshot of the RAM at any point and later restore it. It allows you save in games where it is officially not possible. A potential use case for developers is to temporarily downgrade/update the firmware (in RAM) in order test things (this has not been tested yet).

## Requirements

A Memory Card is required for this plugin.

## Installation

1. Download [dejavu.skprx](https://github.com/TheOfficialFloW/dejavu/releases) and copy it to `ux0:tai/dejavu.skprx`.

2. Edit `ux0:tai/config.txt` and add its path to `*KERNEL` as follows:

   ```
   *KERNEL
   ux0:tai/dejavu.skprx
   ```

3. Reboot your device and enjoy this new feature.

## Save/Load State Procedure

You can save or a load a state at any point by pressing a combination of three buttons.

- To **save** a state, press the **R** trigger.
- To **load** a state, press the **L** trigger.

Then, select one of the six slots by pressing them:

- SELECT
- START
- TRIANGLE
- CIRCLE
- CROSS
- SQUARE

Finally initiate the procedure by pressing the **PS button** while holding the previous two buttons. At this point, the device will do a soft reset and save or load the state while **the screen remains black**. Saving a state takes around **60s** while loading a state takes around **40s**. Depending on the file system, it can take even longer. On finish, the device will resume to the unlock screen. If you do not pay attention to your device, it may go into standby mode after a while. In that case, press the power button to resume your device manually. During the save and load procedure, **the game card LED should flash**.

A snapshot of a game is around **400-500MB** big and will be written to the Memory Card at `ux0:savestate`. When saving a state, a backup of the savedata  will made and when loading a state, the current savedata will be replaced with the old savedata.

## Caveats

Loading a state is not yet 100% stable.

- **Your SD Card and/or Memory Card may be corrupted and you may lose your data.**
- The system may crash.

## Notice

- This plugin is only experimental and may contain bugs. If you encounter any, please give a detailed feedback in [Issues](https://github.com/TheOfficialFloW/dejavu/issues).
- This plugin has only been tested on a PS Vita Slim on firmware 3.65.

## Technical Details

This plugin makes use of the soft reset feature to enter a state where all resources are suspended. At this point, a payload is executed which is mapped to address `0x1C000000`. The payload flushes and disables all the caches as well as the MMU. Using [FatFs](http://elm-chan.org/fsw/ff/00index_e.html) and a reverse engineered and reimplemented Memory Card driver, the RAM and a few other states are written to Memory Card. In order to reduce the size of the snapshot, only sections and pages that are referenced by the page table are stored. When loading the state, the same procedure happens, however instead of writing, all the memory is written back. A big challenge of this plugin is to refresh the file system after loading a state. Namely, files may not exist anymore, they may be stored in different clusters or they may have a different size. All this must all be updated accordingly. Moreover, file system caches must be invalidated. This problem has still not been solved properly and hence, corruptions and crashes may still happen.

## Donation

If you enjoy my work and would like to support me, you can do it on [patreon](https://www.patreon.com/TheOfficialFloW). Many thanks!

## Credits

- Thanks to [xerpi](https://github.com/xerpi) and [motoharu](https://github.com/motoharu-gosuto) for their Memory Card research.
- Thanks to [xerpi](https://github.com/xerpi)for [vita-libbaremetal](https://github.com/xerpi/vita-libbaremetal).
- Thanks to [Team Molecule](https://github.com/TeamMolecule) for their prior research.
- Thanks to Dark_AleX for pspstates which inspired me to work on this project.

