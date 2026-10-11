#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace chip8 {

enum class Profile : std::uint8_t { Vip, Modern, SuperChip, XoChip };

struct Config {
    Profile profile = Profile::Modern;
    bool shiftUsesVx = true;
    bool incrementI = false;
    bool logicResetsVf = false;
    bool jumpUsesVx = false;
    bool clipSprites = false;
    bool drawWait = false;
    bool keyRelease = true;
    static Config preset(Profile profile);
    std::uint8_t quirks() const;
    static Config fromQuirks(Profile profile, std::uint8_t bits);
    bool extended() const { return profile == Profile::SuperChip || profile == Profile::XoChip; }
    bool xo() const { return profile == Profile::XoChip; }
};

const char* profileName(Profile profile);

enum class DisplayUpdate : std::uint8_t { Other, DrawErase, DrawAdd };

// Presentation metadata for the most recent DXYN. It is not guest state and
// never participates in collision tests or save-state serialization.
struct SpriteUpdate {
    std::uint16_t opcode = 0;
    std::uint8_t x = 0, y = 0, width = 8, rows = 0, planes = 1;
    bool clipped = false;
    std::array<std::uint8_t, 64> bytes{};
};

class Chip8 {
public:
    static constexpr std::size_t MemorySize = 65536;
    static constexpr std::size_t BaseMemorySize = 4096;
    static constexpr std::size_t ProgramStart = 0x200;
    static constexpr std::size_t DisplayWidth = 128; // storage stride; use width()/height()
    static constexpr std::size_t DisplayHeight = 64;
    static constexpr std::size_t DisplaySize = DisplayWidth * DisplayHeight;
    static constexpr std::size_t StackSize = 16;
    static constexpr std::size_t KeyCount = 16;
    using Display = std::array<std::uint8_t, DisplaySize>;

    explicit Chip8(Config config = Config::preset(Profile::Modern));
    void reset();
    bool loadRom(const std::vector<std::uint8_t>& rom);
    bool loadRom(const std::uint8_t* bytes, std::size_t size);
    bool step(); // false on halt; key/vblank waits return true without executing
    void tickTimers(); // exactly once per emulated 1/60 s, also releases vblank gate
    void setKey(std::uint8_t key, bool pressed);
    void releaseKeys(); // focus changes must not complete FX0A artificially
    bool isKeyPressed(std::uint8_t key) const;
    void setConfig(Config config) { config_ = config; }
    const Config& config() const { return config_; }
    void seedRandom(std::uint32_t seed);
    void setFlags(const std::array<std::uint8_t, 16>& flags) { flags_ = flags; }

    const std::array<std::uint8_t, MemorySize>& memory() const { return memory_; }
    const std::array<std::uint8_t, 16>& registers() const { return v_; }
    const std::array<std::uint16_t, StackSize>& stack() const { return stack_; }
    const Display& display() const { return display_; }
    const std::array<std::uint8_t, 16>& flags() const { return flags_; }
    const std::array<std::uint8_t, 16>& audioPattern() const { return audio_; }
    std::uint8_t pitch() const { return pitch_; }
    int width() const { return hires_ ? 128 : 64; }
    int height() const { return hires_ ? 64 : 32; }
    std::size_t memoryLimit() const { return config_.xo() ? MemorySize : BaseMemorySize; }
    std::uint64_t displayGeneration() const { return generation_; }
    DisplayUpdate displayUpdate() const { return display_update_; }
    const SpriteUpdate& spriteUpdate() const { return sprite_update_; }
    std::uint16_t index() const { return i_; }
    std::uint16_t pc() const { return pc_; }
    std::uint8_t delayTimer() const { return dt_; }
    std::uint8_t soundTimer() const { return st_; }
    std::uint8_t stackDepth() const { return sp_; }
    bool waitingForKey() const { return waiting_key_; }
    bool waitingForDraw() const { return waiting_draw_; }
    bool halted() const { return halted_; }
    const std::string& error() const { return error_; }
    std::uint64_t instructionCount() const { return instructions_; }
    std::uint64_t romHash() const { return rom_hash_; }
    std::size_t romSize() const { return rom_size_; }
    bool poke(std::uint16_t address, std::uint8_t value);
    std::string disassemble(std::uint16_t address) const;
    void clearDisplay();

    // Versioned, little-endian format: no raw C++ object/padding/pointers.
    // Restoring is atomic and rejects corruption, invalid state and another ROM.
    std::vector<std::uint8_t> saveState() const;
    bool loadState(const std::vector<std::uint8_t>& data, std::string& error);

private:
    bool fail(const char* message, std::uint16_t address, std::uint16_t opcode);
    bool range(std::size_t address, std::size_t count) const;
    bool skip(std::uint16_t address, std::uint16_t opcode);
    bool draw(std::uint8_t vx, std::uint8_t vy, std::uint8_t rows,
              std::uint16_t address, std::uint16_t opcode);
    void scroll(int dx, int dy);
    void changed(DisplayUpdate kind = DisplayUpdate::Other);
    std::uint8_t randomByte();

    Config config_;
    std::array<std::uint8_t, MemorySize> memory_{};
    std::array<std::uint8_t, 16> v_{}, flags_{}, audio_{};
    std::array<std::uint16_t, StackSize> stack_{};
    Display display_{};
    std::array<bool, KeyCount> keys_{};
    std::uint16_t i_ = 0, pc_ = ProgramStart;
    std::uint8_t sp_ = 0, dt_ = 0, st_ = 0, plane_ = 1, pitch_ = 64;
    bool hires_ = false, halted_ = false, waiting_key_ = false;
    bool waiting_draw_ = false, draw_ready_ = false;
    std::uint8_t key_reg_ = 0;
    int pending_key_ = -1;
    std::uint32_t rng_ = 0xC0FFEE12;
    std::uint64_t generation_ = 0, instructions_ = 0, rom_hash_ = 0;
    std::size_t rom_size_ = 0;
    DisplayUpdate display_update_ = DisplayUpdate::Other;
    SpriteUpdate sprite_update_{};
    std::string error_;
};

// CPU and timer events are processed in timestamp order. Changing the host
// polling rate does not change emulation. Long host stalls are bounded.
class Scheduler {
public:
    explicit Scheduler(double hz = 700) { setHz(hz); }
    void setHz(double hz);
    double hz() const { return hz_; }
    void reset();
    void advance(Chip8& cpu, double seconds, const std::function<void()>& onFrame = {},
                 const std::function<void()>& onDisplay = {});
    double untilNextEvent() const;
    double untilNextFrame() const { return frame_left_; }
private:
    double hz_ = 700, cpu_left_ = 1.0 / 700.0, frame_left_ = 1.0 / 60.0;
};

// Observe every display instruction, then publish immutable snapshots at 60 Hz.
// In stabilized mode each erased sprite is kept until its matching redraw.
// An unmatched erasure expires within four frame boundaries (at most 66.7 ms).
// Raw guest pixels, timing and collision flags are never changed.
class Presenter {
public:
    void observe(const Chip8& cpu);
    bool publish(const Chip8& cpu, bool stabilize, bool force = false);
    const Chip8::Display& display() const { return display_; }
    int width() const { return width_; }
    int height() const { return height_; }
    void reset() { generation_ = ~std::uint64_t{0}; pending_.clear(); }
private:
    struct HeldPixel { std::uint16_t index; std::uint8_t plane; };
    struct PendingSprite {
        std::uint16_t key;
        std::uint8_t planes, age = 0, x = 0, y = 0;
        std::vector<HeldPixel> pixels;
    };
    Chip8::Display display_{};
    std::vector<PendingSprite> pending_;
    std::uint64_t generation_ = ~std::uint64_t{0};
    int width_ = 64, height_ = 32;
};

} // namespace chip8
