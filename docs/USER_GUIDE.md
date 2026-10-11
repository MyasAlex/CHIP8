# CHIP-8 Studio: player guide

## Download and launch

Download **CHIP8-Studio-Windows-x64.zip** from the [latest release](https://github.com/MyasAlex/CHIP8/releases/latest), extract the whole archive and open **CHIP8.Emulator.exe**. The target system is Windows 10/11 x64. Games and the demo are embedded in the executable.

The application interface is currently Russian. Useful menu names are **Файл** (File), **Игры** (Games), **Экран** (Display) and **Эмуляция** (Emulation).

Two-player Pong starts on launch. Player one uses **W/S**, player two uses **↑/↓**. In **Игры**, choose classic Pong for the original ROM's opponent, Breakout or the animated demo. Two-player Pong has no built-in bot.

## Load another game

Use **Ctrl+O**, drag a ROM onto the window, or pass its path on the command line. Extensions .ch8, .rom, .sc8 and .xo8 are accepted. .sc8 selects Super-CHIP; .xo8 and ROMs larger than 3584 bytes select XO-CHIP. An ordinary .ch8 starts with Modern unless its saved settings choose another profile. There is no automatic hash-based compatibility database.

## Keyboard and controllers

| Action | Key |
| --- | --- |
| Open ROM | Ctrl+O |
| Pause / restart | P / F1 |
| Save state / load state | F5 / F8 |
| Rewind | Backspace |
| Save PNG | F6 |
| Step a frame / instruction | F9 / F10 |
| Fullscreen / leave fullscreen | F11 / Esc |
| Debug panel / mute | F12 / M |

CHIP-8 keys **123C / 456D / 789E / A0BF** use the physical keyboard rows **1234 / QWER / ASDF / ZXCV**. The game itself chooses which keys it uses.

For two-player Pong, the first XInput controller moves the left paddle and the second moves the right paddle with D-pad up/down or the left stick. In other ROMs, D-pad/stick map to 5/8/7/9, and A/B/X/Y/Start map to 6/4/0/F/A.

## Speed, profiles and flicker

Start with the game's saved defaults. Classic Pong and Breakout are configured for 700 instructions per second; the original two-player Pong uses 6000. Changing CPU speed changes how quickly a ROM executes, while timers remain at 60 Hz.

Select VIP, Modern, Super-CHIP Modern or XO-CHIP to match the ROM's requirements. Changing instruction quirks restarts the ROM. Profile, quirks and speed are remembered per ROM.

XOR stabilization is enabled by default. It holds an erased sprite briefly until its matching redraw, reducing visible gaps in some old games. For test ROMs or deliberate blinking, select the exact display mode. Stabilization is a presentation heuristic and does not alter the guest's collision flags or memory; unmatched erasures can remain visible for up to 66.7 ms.

## Where files go

Settings, state files, RPL flags and PNG screenshots are stored in **%LOCALAPPDATA%\\CHIP8Studio**. The **Файл → Папка данных** command opens the data folder. If Local AppData is unavailable, the application uses userdata beside the executable.

F5 stores the current ROM's state; F8 validates and restores it. Corrupt saves and saves from another ROM are rejected. CPU speed follows the current application settings. Rewind keeps up to 31 in-memory snapshots about one second apart.

## If something looks wrong

| Symptom | Try |
| --- | --- |
| Controls appear unresponsive | Click the game window, check the ROM's key instructions and press P if paused. |
| An old game flickers | Enable XOR stabilization and try the game's original CPU speed and profile. |
| A test ROM's intentional flashing looks different | Select the exact display. |
| A ROM stops with an instruction error | Check its required profile and extension, then include the error and ROM details in a report. |
| No sound | Press M, check Windows output volume and the active output device. Sound is only generated while a ROM's sound timer is active. |

For an unresolved issue, [open a bug report](https://github.com/MyasAlex/CHIP8/issues/new?template=bug_report.yml). Include the release version, Windows version, ROM name or legal source link, profile, CPU speed and reproduction steps. A video or PNG helps explain display problems.
