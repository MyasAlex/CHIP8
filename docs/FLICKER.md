# Why old moving sprites flicker

CHIP-8 draws with XOR. A ROM can erase a sprite by drawing it at its old position, then update coordinates and draw at its new position. Those operations can span a display frame. Showing the guest framebuffer in between makes the object temporarily disappear even when the ROM is intact.

Studio 2.1 observes every display-changing instruction. For a pure sprite erasure, the presentation layer keeps its pixels while waiting for the matching redraw. Matching uses the draw opcode, sprite data and planes, with the nearest old position resolving otherwise identical candidates. A redraw replaces the held position instead of accumulating the old and new images.

The presentation layer is separate from guest memory, display pixels, collision flags, timers and state serialization. Clear, scroll, resolution changes and state restoration reset pending display data. If a redraw never occurs, the hold expires within four frame boundaries (at most 66.7 ms).

This is a visual heuristic. It can conceal a short intentional blink or delay an object's disappearance. **Exact display** bypasses it and is the appropriate mode for conformance tests or inspecting the guest framebuffer.

## Regression evidence

The suite covers independent moving sprites, nearest-candidate matching, clear/scroll/mode changes, two-plane XO-CHIP drawing, exact/forced publication and unchanged guest states across 1200-frame replays of the original games.

At 700 instructions/second, selected normal ball-movement windows contained 700 Pong samples and 277 Breakout samples. The raw sampled display had 389 and 125 missing-ball frames; the stabilized sampled display had zero. These are bounded regression measurements, not a claim that all ROMs never flicker.

The classic Pong and Breakout ROMs were not modified for this fix. Their bytes match the corresponding publicly archived kripod/chip8-roms files. The emulator does not assign a new license to these guest ROMs; their retained notices are documented in [THIRD_PARTY.md](../THIRD_PARTY.md).

The detailed Russian test record is in [FIX_FLICKER_RU.md](FIX_FLICKER_RU.md).
