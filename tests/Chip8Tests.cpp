#include "../src/Chip8Core.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

using chip8::Chip8;

void writeOpcode(std::vector<std::uint8_t>& rom, std::size_t offset, std::uint16_t opcode) {
    assert(offset + 1 < rom.size());
    rom[offset] = static_cast<std::uint8_t>(opcode >> 8);
    rom[offset + 1] = static_cast<std::uint8_t>(opcode & 0xFF);
}

void testLoadAndReset() {
    Chip8 c;
    std::vector<std::uint8_t> rom{0x60, 0x2A};
    assert(c.loadRom(rom));
    assert(c.pc() == Chip8::ProgramStart);
    assert(c.step());
    assert(c.registers()[0] == 0x2A);
    c.reset();
    assert(c.registers()[0] == 0);
    assert(c.pc() == Chip8::ProgramStart);
    assert(c.memory()[0x50] == 0xF0); // font remains installed after reset
}

void testArithmeticFlags() {
    Chip8 c;
    std::vector<std::uint8_t> rom(14, 0);
    writeOpcode(rom, 0, 0x60FF);
    writeOpcode(rom, 2, 0x6101);
    writeOpcode(rom, 4, 0x8014); // V0 += V1 -> 0, carry
    writeOpcode(rom, 6, 0x6001);
    writeOpcode(rom, 8, 0x6102);
    writeOpcode(rom, 10, 0x8015); // 1 - 2 -> FF, no borrow flag
    assert(c.loadRom(rom));
    c.step(); c.step(); c.step();
    assert(c.registers()[0] == 0x00 && c.registers()[0xF] == 1);
    c.step(); c.step(); c.step();
    assert(c.registers()[0] == 0xFF && c.registers()[0xF] == 0);
}

void testShiftFlags() {
    Chip8 c;
    std::vector<std::uint8_t> rom(6, 0);
    writeOpcode(rom, 0, 0x60FF);
    writeOpcode(rom, 2, 0x8006); // SHR V0
    writeOpcode(rom, 4, 0x800E); // SHL V0
    assert(c.loadRom(rom));
    c.step();
    c.step();
    assert(c.registers()[0] == 0x7F && c.registers()[0xF] == 1);
    c.step();
    assert(c.registers()[0] == 0xFE && c.registers()[0xF] == 0);
}

void testCallReturn() {
    Chip8 c;
    std::vector<std::uint8_t> rom(10, 0);
    writeOpcode(rom, 0, 0x2206); // CALL 206
    writeOpcode(rom, 2, 0x6007); // after return
    writeOpcode(rom, 6, 0x600A); // subroutine
    writeOpcode(rom, 8, 0x00EE); // return
    assert(c.loadRom(rom));
    assert(c.step());
    assert(c.stackDepth() == 1);
    assert(c.pc() == 0x206);
    assert(c.step());
    assert(c.registers()[0] == 10);
    assert(c.step());
    assert(c.stackDepth() == 0);
    assert(c.pc() == 0x202);
    assert(c.step());
    assert(c.registers()[0] == 7);
}

void testDisplayAndCollision() {
    Chip8 c;
    // Program bytes occupy 0x200..0x209. Put one sprite byte at 0x300.
    std::vector<std::uint8_t> rom(0x101, 0);
    writeOpcode(rom, 0, 0x6000); // V0=0
    writeOpcode(rom, 2, 0x6100); // V1=0
    writeOpcode(rom, 4, 0xA300); // I=300
    writeOpcode(rom, 6, 0xD011); // draw 1 row
    writeOpcode(rom, 8, 0xD011); // draw again
    rom[0x100] = 0x80;
    assert(c.loadRom(rom));
    c.step(); c.step(); c.step(); c.step();
    assert(c.display()[0]);
    assert(c.registers()[0xF] == 0);
    c.step();
    assert(!c.display()[0]);
    assert(c.registers()[0xF] == 1);
}

void testSkipInstructions() {
    Chip8 c;
    std::vector<std::uint8_t> rom(10, 0);
    writeOpcode(rom, 0, 0x6001);
    writeOpcode(rom, 2, 0x3001); // skip next
    writeOpcode(rom, 4, 0x6002); // skipped
    writeOpcode(rom, 6, 0x6103);
    writeOpcode(rom, 8, 0x4104); // not equal -> skip if V1 != 4
    assert(c.loadRom(rom));
    c.step(); c.step();
    assert(c.pc() == 0x206);
    c.step();
    assert(c.registers()[1] == 3);
    c.step();
    assert(c.pc() == 0x20C);
}

void testTimers() {
    Chip8 c;
    std::vector<std::uint8_t> rom{0x60, 0x03, 0xF0, 0x15, 0xF0, 0x07};
    assert(c.loadRom(rom));
    c.step();
    c.step();
    assert(c.delayTimer() == 3);
    c.tickTimers();
    assert(c.delayTimer() == 2);
    c.tickTimers();
    assert(c.delayTimer() == 1);
    c.tickTimers();
    assert(c.delayTimer() == 0);
    c.step();
    assert(c.registers()[0] == 0);
}

void testSoundTimer() {
    Chip8 c;
    std::vector<std::uint8_t> rom{0x60, 0x02, 0xF0, 0x18};
    assert(c.loadRom(rom));
    c.step();
    c.step();
    assert(c.soundTimer() == 2);
    c.tickTimers();
    assert(c.soundTimer() == 1);
    c.tickTimers();
    assert(c.soundTimer() == 0);
}

void testBCD() {
    Chip8 c;
    std::vector<std::uint8_t> rom(6, 0);
    writeOpcode(rom, 0, 0x607B); // 123
    writeOpcode(rom, 2, 0xA300);
    writeOpcode(rom, 4, 0xF033);
    assert(c.loadRom(rom));
    c.step(); c.step(); c.step();
    assert(c.memory()[0x300] == 1);
    assert(c.memory()[0x301] == 2);
    assert(c.memory()[0x302] == 3);
}

void testStoreLoad() {
    Chip8 c;
    std::vector<std::uint8_t> rom(12, 0);
    writeOpcode(rom, 0, 0x600A);
    writeOpcode(rom, 2, 0x610B);
    writeOpcode(rom, 4, 0xA300);
    writeOpcode(rom, 6, 0xF155);
    writeOpcode(rom, 8, 0x6000);
    writeOpcode(rom, 10, 0xF165);
    assert(c.loadRom(rom));
    for (int i = 0; i < 6; ++i) assert(c.step());
    assert(c.memory()[0x300] == 0x0A);
    assert(c.memory()[0x301] == 0x0B);
    assert(c.registers()[0] == 0x0A);
    assert(c.registers()[1] == 0x0B);
}

void testFont() {
    Chip8 c;
    std::vector<std::uint8_t> rom{0x60, 0x0A, 0xF0, 0x29};
    assert(c.loadRom(rom));
    c.step(); c.step();
    assert(c.index() == 0x50 + 10 * 5);
    assert(c.memory()[c.index()] == 0xF0);
}

void testFx0aWaitForKey() {
    Chip8 c;
    std::vector<std::uint8_t> rom{0xF2, 0x0A, 0x60, 0x09};
    assert(c.loadRom(rom));
    assert(c.step());
    assert(c.waitingForKey());
    assert(c.pc() == Chip8::ProgramStart);
    c.setKey(0xB, true);
    c.setKey(0xB, false);
    assert(!c.waitingForKey());
    assert(c.registers()[2] == 0xB);
    assert(c.pc() == Chip8::ProgramStart + 2);
    assert(c.step());
    assert(c.registers()[0] == 9);
}

void testRandomOpcode() {
    Chip8 c;
    std::vector<std::uint8_t> rom{0xC0, 0xFF};
    assert(c.loadRom(rom));
    assert(c.step());
    (void)c.registers()[0];
}

void testProgramCounterBounds() {
    Chip8 c;
    std::vector<std::uint8_t> rom{0x1F, 0xFF};
    assert(c.loadRom(rom));
    assert(c.step());
    assert(!c.step());
    assert(c.halted());
    assert(!c.error().empty());
}

int main() {
    testLoadAndReset();
    testArithmeticFlags();
    testShiftFlags();
    testCallReturn();
    testDisplayAndCollision();
    testSkipInstructions();
    testTimers();
    testSoundTimer();
    testBCD();
    testStoreLoad();
    testFont();
    testFx0aWaitForKey();
    testRandomOpcode();
    testProgramCounterBounds();

    std::cout << "All CHIP-8 core tests passed. 14 test groups.\n";
    return 0;
}
