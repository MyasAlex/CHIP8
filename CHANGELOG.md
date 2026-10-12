# Changelog

## 3.0.0

- Add an offline library of 210 unique ROM programs with original notices/covers, SHA-1 profiles, search, favorites, recent launches, playtime and managed multi-ROM/folder import.
- Add exact/stable/phosphor modes, optional CRT effects, eight palettes and ROM-provided colors without modifying guest state.
- Add ten slots per ROM, autosave/resume, last-game startup, bounded delta rewind up to 120 seconds, timeline dragging and continuous Backspace rewind.
- Add turbo/slow motion, per-ROM keyboard remapping, virtual keypad, local Pong achievements and FPS/process CPU metrics.
- Add streaming GIF, dynamically loaded H.264/AAC MP4, and reset-based input recording/replay with per-frame checksums.
- Add breakpoints, HEX memory browsing, bounded instruction trace, register change highlighting, comparison PNG and diagnostic report export.
- Add validated settings/save/managed-ROM transfer and explicit portable mode.
- Retain 31 Timendus references and expand to 51 core test groups, all 199 launchable catalog boot checks, independent GIF and native MP4/AAC decoding, and extended Windows smoke checks.

The catalog includes demos/tests, is not a complete historical archive, and marks 11 entries requiring other platforms. CHIP-8X, Super-CHIP Legacy, cycle-level VIP/HP48 and the game editor remain future work.

## 2.1.0

- Track XOR erasure and redraw per sprite, reducing flicker in classic Pong and Breakout without changing the guest's memory, collision flags or instruction timing.
- Replace the old sprite position on redraw; clear pending presentation data on clear, scroll, mode changes and state restoration.
- Keep an exact display mode available. Unmatched erasures expire within four frame boundaries, at most 66.7 ms.
- Add regression coverage for independently moving sprites, nearest redraw matching, display resets, XO-CHIP planes and unchanged guest state across original-game replays.
- Provide a portable Windows release, bilingual player documentation, reproducible packaging, Windows/Linux CI and native Windows smoke checks.

## 2.0.0 — development baseline

- Replace the original core with a portable C++20 implementation and separate VIP, Modern, Super-CHIP Modern and XO-CHIP profiles.
- Add save states with validation, rewind, PNG screenshots, fullscreen, debugging, XInput support and XO-CHIP audio.
- Add an original two-player Pong ROM and an animated demo, embedded alongside the existing classic games.
- Publish immutable frame snapshots at 60 Hz and separate CPU scheduling from the display.
- Add core tests and offline Timendus framebuffer comparisons.

Version 2.0 was a development build and was not published as a GitHub release.
