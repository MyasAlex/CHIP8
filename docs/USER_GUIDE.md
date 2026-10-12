# CHIP-8 Studio 3.0: player guide

Download the [Windows x64 ZIP](https://github.com/MyasAlex/CHIP8/releases/latest/download/CHIP8-Studio-Windows-x64.zip), extract **the whole archive**, including `roms/catalog`, and open **CHIP8.Emulator.exe**. First launch starts two-player Pong: W/S moves the left paddle, Up/Down the right. Later launches can resume the last game from its automatic save.

Press **Библиотека** or **Ctrl+L** for the offline library: search, covers, favorites, recently played programs and playtime. The catalog has 210 unique games, demos and programs; original Studio programs bring the library to 212 entries. Eleven catalog entries require other platforms and are marked accordingly. It is not an exhaustive historical archive.

The interface is Russian. Menu translations: **Файл** = File, **Игры** = Games, **Эмуляция** = Emulation, **Экран** = Display, **Запись** = Recording, **Диагностика** = Diagnostics.

| Action | Key |
| --- | --- |
| Open one ROM / import several | Ctrl+O; also drag and drop |
| Library | Ctrl+L |
| Pause / restart from the beginning | P / F1 |
| Save / load selected slot | F5 / F8 |
| Rewind | Hold Backspace or drag the timeline |
| Turbo 4× / slow motion 0.25× | Hold Tab / Ctrl |
| Screenshot / fullscreen | F6 / F11 |
| Debug / instruction / frame | F12 / F10 / F9 |
| Mute | M |

## Games and controls

The four embedded programs work without catalog files. The full library needs the bundled `roms/catalog` directory. Library pages change with the mouse wheel or bottom arrows; right-click a card for its description. Click its star to toggle a favorite.

Open/import accepts .ch8, .rom, .sc8 and .xo8. Known ROM content selects profiles, quirks, CPU speed, colors and controls from 547 SHA-1 fingerprints. Saved user choices take priority. Unknown ROMs use extension/size defaults, which can be changed in Emulation.

Physical rows `1234 / QWER / ASDF / ZXCV` map to CHIP-8 `123C / 456D / 789E / A0BF`. For known programs, arrows/Space/Enter and XInput directions/A/B also use the database controls. Two-player Pong has no bot; the first XInput pad moves the left paddle and the second the right.

**Эмуляция → Переназначить 16 клавиш** captures sixteen unique physical keys, 0 through F. Esc cancels the change. It is saved per ROM and replaces the automatic keyboard shortcuts for game input. P, M, Tab, Ctrl, Backspace, Esc and function keys remain application commands. **Виртуальная клавиатура** enables a mouse-operated keypad.

## Display and timing

**Оригинал** shows the exact display and disables visual effects. **Стабильный** stabilizes intermediate XOR erasures; it is the default. **Плавный** adds a short phosphor trail. **CRT** independently enables scanlines, glow and vignette with adjustable intensity. Eight manual palettes and ROM-provided colors are available.

These effects never alter guest memory or collision flags. Stabilization can delay an unmatched disappearance by up to 66.7 ms; phosphor intentionally creates short trails. Use Exact for tests and Stable for ordinary play without trails.

Turbo/slow motion scale the whole virtual clock, including timers and input. CPU speed selects instructions per virtual second; guest timers remain 60 Hz. FPS reports virtual frames per real second. CPU usage is process time divided by system processing capacity, not a hardware VSync measurement.

## Saves and rewind

File selects one of ten slots per ROM for F5/F8. New saves have a PNG preview beside the state file. Slot one can read the legacy single-state file from 2.1. Corrupt and wrong-ROM states are rejected.

Autosave runs every 30 virtual seconds and on ROM changes/window close. Automatic saving and resuming are separate menu options. F1 always restarts from the beginning.

The timeline captures 30 snapshots per virtual second. Select a 30/60/120-second limit in Emulation; compressed storage is bounded to 64 MiB, so heavy memory changes can shorten it. Hold Backspace for roughly 4× rewind, or drag the timeline. Releasing it resumes the previous pause state and discards the future branch. Input is released; presentation holds are rebuilt when execution resumes.

## Recording and diagnosis

Recording supports GIF (256×128, 256 colors, 30 FPS) and H.264/AAC MP4 (512×256, mono 48 kHz, 30 FPS). Stop from Recording; ROM changes, rewind and closing also finalize the file. Limit: approximately two virtual minutes, plus 128 MiB for GIF. Pauses are omitted; speed modes change how quickly virtual recording time accumulates. MP4 requires installed Windows Media Foundation codecs; GIF remains available if they are absent.

Input recording restarts the ROM, stores initial state/RNG, speed, key masks and per-frame checksums, and writes a `.c8replay` when stopped. Limit: twenty minutes. Replay includes the ROM, ignores physical game input and checks every frame. Divergence stops execution with a frame number. Profile/speed changes, state loading and rewind finish input recording/playback.

Diagnostics adds HEX-address breakpoints, memory browsing, register change highlights, the last 1024 instruction attempts while debugging, comparison PNGs and text reports. Comparison PNG: exact guest screen on the left, stabilized screen on the right, with effects omitted.

## Data and transfer

Data is in `%LOCALAPPDATA%\CHIP8Studio`; **Файл → Папка данных и снимков** opens it. Portable mode transfers data to `userdata` beside the EXE and creates `portable.ini`; disabling it copies data back. Old folders are retained.

Settings export creates a `.c8pack` with settings, states, RPL and managed imported ROMs, up to 64 MiB. The shared catalog travels with the application ZIP. Screenshots, videos and input replays are not included. Import verifies checksum and permitted filenames, replaces matching files and reloads the current game.

CHIP-8X, Super-CHIP Legacy, MegaChip, cycle-level VIP/HP48 and a game editor are not implemented. Catalog boot tests do not verify full game playthroughs. See the [complete Russian guide](STUDIO_3_RU.md) and [ROM notices](../THIRD_PARTY.md). For a problem, export a report and input replay if possible, then [open an issue](https://github.com/MyasAlex/CHIP8/issues/new?template=bug_report.yml).
