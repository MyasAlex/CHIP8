#pragma once

#include <cstdint>
#include <vector>

namespace chip8_demo {

inline std::vector<std::uint8_t> makeRom() {
    // A tiny original test ROM written for this project.
    // It clears the screen, draws a CHIP-8 logo-like pattern using I=0x300,
    // then loops. This is intentionally not a copyrighted game ROM.
    return {
        0x00, 0xE0,       // CLS
        0x60, 0x00,       // V0 = 0
        0x61, 0x00,       // V1 = 0
        0xA2, 0x10,       // I = 0x210 (sprite immediately follows the program)
        0xD0, 0x18,       // draw 8x24 sprite at V0,V1
        0x70, 0x20,       // V0 += 32
        0xD0, 0x18,       // draw again
        0x12, 0x0E,       // loop forever at this point
        // 0x210: simple 8x24 pattern (filled border / center)
        0xFF, 0x81, 0xBD, 0xA5, 0xA5, 0xBD, 0x81, 0xFF,
        0x81, 0x99, 0xA5, 0x81, 0x81, 0xA5, 0x99, 0x81,
        0xFF, 0x81, 0x99, 0x81, 0x81, 0x99, 0x81, 0xFF
    };
}

} // namespace chip8_demo
