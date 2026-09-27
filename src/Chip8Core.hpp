#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace chip8 {

class Chip8 {
public:
    static constexpr std::size_t MemorySize = 4096;
    static constexpr std::size_t ProgramStart = 0x200;
    static constexpr std::size_t DisplayWidth = 64;
    static constexpr std::size_t DisplayHeight = 32;
    static constexpr std::size_t DisplaySize = DisplayWidth * DisplayHeight;
    static constexpr std::size_t StackSize = 16;
    static constexpr std::size_t KeyCount = 16;

    Chip8();

    void reset();
    bool loadRom(const std::vector<std::uint8_t>& rom);
    bool loadRom(const std::uint8_t* data, std::size_t size);

    // Executes one CHIP-8 instruction. Returns false when the CPU is halted.
    bool step();
    void tickTimers(); // 60 Hz timers

    void setKey(std::uint8_t key, bool pressed);
    bool isKeyPressed(std::uint8_t key) const;

    const std::array<std::uint8_t, MemorySize>& memory() const { return memory_; }
    const std::array<std::uint8_t, 16>& registers() const { return v_; }
    const std::array<bool, DisplaySize>& display() const { return display_; }
    std::uint64_t displayGeneration() const { return display_generation_; }
    bool drawPending() const { return draw_pending_; }
    void clearDrawPending() { draw_pending_ = false; }
    std::uint16_t index() const { return i_; }
    std::uint16_t pc() const { return pc_; }
    std::uint8_t delayTimer() const { return delay_timer_; }
    std::uint8_t soundTimer() const { return sound_timer_; }
    std::uint8_t stackDepth() const { return sp_; }
    bool waitingForKey() const { return waiting_for_key_; }
    bool halted() const { return halted_; }
    const std::string& error() const { return error_; }

    void clearDisplay();

private:
    void setError(const std::string& message);
    bool validKeyRegister(std::uint8_t key_value);
    std::uint8_t randomByte();

    std::array<std::uint8_t, MemorySize> memory_{};
    std::array<std::uint8_t, 16> v_{};
    std::uint16_t i_ = 0;
    std::uint16_t pc_ = static_cast<std::uint16_t>(ProgramStart);
    std::array<std::uint16_t, StackSize> stack_{};
    std::uint8_t sp_ = 0;
    std::uint8_t delay_timer_ = 0;
    std::uint8_t sound_timer_ = 0;
    std::array<bool, DisplaySize> display_{};
    std::uint64_t display_generation_ = 0;
    bool draw_pending_ = false;
    std::array<bool, KeyCount> keys_{};

    bool waiting_for_key_ = false;
    std::uint8_t waiting_register_ = 0;
    bool halted_ = false;
    std::string error_;

    std::uint32_t rng_state_ = 0xC0FFEE12u;

    static constexpr std::array<std::uint8_t, 80> Font = {
        // 0
        0xF0, 0x90, 0x90, 0x90, 0xF0,
        // 1
        0x20, 0x60, 0x20, 0x20, 0x70,
        // 2
        0xF0, 0x10, 0xF0, 0x80, 0xF0,
        // 3
        0xF0, 0x10, 0xF0, 0x10, 0xF0,
        // 4
        0x90, 0x90, 0xF0, 0x10, 0x10,
        // 5
        0xF0, 0x80, 0xF0, 0x10, 0xF0,
        // 6
        0xF0, 0x80, 0xF0, 0x90, 0xF0,
        // 7
        0xF0, 0x10, 0x20, 0x40, 0x40,
        // 8
        0xF0, 0x90, 0xF0, 0x90, 0xF0,
        // 9
        0xF0, 0x90, 0xF0, 0x10, 0xF0,
        // A
        0xF0, 0x90, 0xF0, 0x90, 0x90,
        // B
        0xE0, 0x90, 0xE0, 0x90, 0xE0,
        // C
        0xF0, 0x80, 0x80, 0x80, 0xF0,
        // D
        0xE0, 0x90, 0x90, 0x90, 0xE0,
        // E
        0xF0, 0x80, 0xF0, 0x80, 0xF0,
        // F
        0xF0, 0x80, 0xF0, 0x80, 0x80
    };
};

} // namespace chip8
