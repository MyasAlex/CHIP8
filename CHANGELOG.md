# Changelog

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
