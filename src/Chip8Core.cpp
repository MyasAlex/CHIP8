#include "Chip8Core.hpp"

#include <algorithm>
#include <cstring>
#include <sstream>

namespace chip8 {

Chip8::Chip8() {
    reset();
}

void Chip8::reset() {
    memory_.fill(0);
    v_.fill(0);
    stack_.fill(0);
    display_.fill(false);
    keys_.fill(false);
    i_ = 0;
    pc_ = static_cast<std::uint16_t>(ProgramStart);
    sp_ = 0;
    delay_timer_ = 0;
    sound_timer_ = 0;
    waiting_for_key_ = false;
    waiting_register_ = 0;
    halted_ = false;
    error_.clear();
    rng_state_ = 0xC0FFEE12u;
    draw_pending_ = false;

    std::copy(Font.begin(), Font.end(), memory_.begin() + 0x50);
    clearDisplay();
}

void Chip8::clearDisplay() {
    display_.fill(false);
    ++display_generation_;
    draw_pending_ = true;
}

bool Chip8::loadRom(const std::vector<std::uint8_t>& rom) {
    return loadRom(rom.data(), rom.size());
}

bool Chip8::loadRom(const std::uint8_t* data, std::size_t size) {
    reset();

    if (data == nullptr && size != 0) {
        setError("ROM data is null.");
        return false;
    }
    if (size > MemorySize - ProgramStart) {
        setError("ROM is too large. Maximum CHIP-8 ROM size is 3584 bytes.");
        return false;
    }

    if (size != 0) {
        std::memcpy(memory_.data() + ProgramStart, data, size);
    }
    return true;
}

void Chip8::setError(const std::string& message) {
    halted_ = true;
    error_ = message;
}

std::uint8_t Chip8::randomByte() {
    // Tiny deterministic xorshift generator. Deterministic randomness helps tests.
    std::uint32_t x = rng_state_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state_ = x;
    return static_cast<std::uint8_t>(x & 0xFFu);
}

bool Chip8::validKeyRegister(std::uint8_t key_value) {
    return key_value < KeyCount;
}

void Chip8::setKey(std::uint8_t key, bool pressed) {
    if (key >= KeyCount) {
        return;
    }
    keys_[key] = pressed;

    // FX0A waits for a key transition. Once a key is pressed, the instruction
    // completes and execution continues with the next instruction.
    if (pressed && waiting_for_key_) {
        v_[waiting_register_] = key;
        waiting_for_key_ = false;
        pc_ = static_cast<std::uint16_t>(pc_ + 2);
    }
}

bool Chip8::isKeyPressed(std::uint8_t key) const {
    return key < KeyCount && keys_[key];
}

void Chip8::tickTimers() {
    if (delay_timer_ > 0) {
        --delay_timer_;
    }
    if (sound_timer_ > 0) {
        --sound_timer_;
    }
}

bool Chip8::step() {
    if (halted_) {
        return false;
    }

    if (waiting_for_key_) {
        return true;
    }

    if (pc_ > MemorySize - 2) {
        setError("Program counter moved outside RAM.");
        return false;
    }

    const std::uint16_t opcode = static_cast<std::uint16_t>(memory_[pc_] << 8 | memory_[pc_ + 1]);
    pc_ = static_cast<std::uint16_t>(pc_ + 2);

    const std::uint8_t x = static_cast<std::uint8_t>((opcode >> 8) & 0x0F);
    const std::uint8_t y = static_cast<std::uint8_t>((opcode >> 4) & 0x0F);
    const std::uint8_t n = static_cast<std::uint8_t>(opcode & 0x000F);
    const std::uint8_t nn = static_cast<std::uint8_t>(opcode & 0x00FF);
    const std::uint16_t nnn = static_cast<std::uint16_t>(opcode & 0x0FFF);

    switch (opcode & 0xF000) {
    case 0x0000:
        if (opcode == 0x00E0) {
            clearDisplay();
        } else if (opcode == 0x00EE) {
            if (sp_ == 0) {
                setError("Stack underflow on RET.");
                return false;
            }
            --sp_;
            pc_ = stack_[sp_];
        }
        // 0NNN is the old RCA 1802 machine-code call and is ignored on modern CHIP-8.
        break;

    case 0x1000:
        pc_ = nnn;
        break;

    case 0x2000:
        if (sp_ >= StackSize) {
            setError("Stack overflow on CALL.");
            return false;
        }
        stack_[sp_++] = pc_;
        pc_ = nnn;
        break;

    case 0x3000:
        if (v_[x] == nn) {
            if (pc_ > MemorySize - 2) {
                setError("Skip moved program counter outside RAM.");
                return false;
            }
            pc_ = static_cast<std::uint16_t>(pc_ + 2);
        }
        break;

    case 0x4000:
        if (v_[x] != nn) {
            if (pc_ > MemorySize - 2) {
                setError("Skip moved program counter outside RAM.");
                return false;
            }
            pc_ = static_cast<std::uint16_t>(pc_ + 2);
        }
        break;

    case 0x5000:
        if (n != 0) {
            setError("Unknown 5XYN opcode.");
            return false;
        }
        if (v_[x] == v_[y]) {
            if (pc_ > MemorySize - 2) {
                setError("Skip moved program counter outside RAM.");
                return false;
            }
            pc_ = static_cast<std::uint16_t>(pc_ + 2);
        }
        break;

    case 0x6000:
        v_[x] = nn;
        break;

    case 0x7000:
        v_[x] = static_cast<std::uint8_t>(v_[x] + nn);
        break;

    case 0x8000:
        switch (n) {
        case 0x0:
            v_[x] = v_[y];
            break;
        case 0x1:
            v_[x] = static_cast<std::uint8_t>(v_[x] | v_[y]);
            break;
        case 0x2:
            v_[x] = static_cast<std::uint8_t>(v_[x] & v_[y]);
            break;
        case 0x3:
            v_[x] = static_cast<std::uint8_t>(v_[x] ^ v_[y]);
            break;
        case 0x4: {
            const std::uint16_t sum = static_cast<std::uint16_t>(v_[x]) + v_[y];
            v_[0xF] = sum > 0xFF ? 1 : 0;
            v_[x] = static_cast<std::uint8_t>(sum & 0xFF);
            break;
        }
        case 0x5:
            v_[0xF] = v_[x] >= v_[y] ? 1 : 0;
            v_[x] = static_cast<std::uint8_t>(v_[x] - v_[y]);
            break;
        case 0x6:
            v_[0xF] = static_cast<std::uint8_t>(v_[x] & 1u);
            v_[x] >>= 1;
            break;
        case 0x7:
            v_[0xF] = v_[y] >= v_[x] ? 1 : 0;
            v_[x] = static_cast<std::uint8_t>(v_[y] - v_[x]);
            break;
        case 0xE:
            v_[0xF] = static_cast<std::uint8_t>((v_[x] >> 7) & 1u);
            v_[x] = static_cast<std::uint8_t>(v_[x] << 1);
            break;
        default:
            setError("Unknown 8XYN opcode.");
            return false;
        }
        break;

    case 0x9000:
        if (n != 0) {
            setError("Unknown 9XYN opcode.");
            return false;
        }
        if (v_[x] != v_[y]) {
            if (pc_ > MemorySize - 2) {
                setError("Skip moved program counter outside RAM.");
                return false;
            }
            pc_ = static_cast<std::uint16_t>(pc_ + 2);
        }
        break;

    case 0xA000:
        i_ = nnn;
        break;

    case 0xB000:
        // Original CHIP-8 behavior: jump to NNN + V0.
        pc_ = static_cast<std::uint16_t>(nnn + v_[0]);
        break;

    case 0xC000:
        v_[x] = static_cast<std::uint8_t>(randomByte() & nn);
        break;

    case 0xD000: {
        v_[0xF] = 0;
        const std::uint8_t vx = v_[x];
        const std::uint8_t vy = v_[y];

        for (std::uint8_t row = 0; row < n; ++row) {
            const std::uint16_t addr = static_cast<std::uint16_t>(i_ + row);
            if (addr >= MemorySize) {
                setError("Sprite read moved outside RAM.");
                return false;
            }
            const std::uint8_t sprite = memory_[addr];
            for (std::uint8_t bit = 0; bit < 8; ++bit) {
                if ((sprite & (0x80u >> bit)) == 0) {
                    continue;
                }
                const std::uint8_t px = static_cast<std::uint8_t>((vx + bit) % DisplayWidth);
                const std::uint8_t py = static_cast<std::uint8_t>((vy + row) % DisplayHeight);
                const std::size_t index = static_cast<std::size_t>(py) * DisplayWidth + px;
                if (display_[index]) {
                    v_[0xF] = 1;
                }
                display_[index] = !display_[index];
            }
        }
        ++display_generation_;
        draw_pending_ = true;
        break;
    }

    case 0xE000:
        if (nn == 0x9E) {
            if (isKeyPressed(v_[x])) {
                if (pc_ > MemorySize - 2) {
                    setError("Skip moved program counter outside RAM.");
                    return false;
                }
                pc_ = static_cast<std::uint16_t>(pc_ + 2);
            }
        } else if (nn == 0xA1) {
            if (!isKeyPressed(v_[x])) {
                if (pc_ > MemorySize - 2) {
                    setError("Skip moved program counter outside RAM.");
                    return false;
                }
                pc_ = static_cast<std::uint16_t>(pc_ + 2);
            }
        } else {
            setError("Unknown EXNN opcode.");
            return false;
        }
        break;

    case 0xF000:
        switch (nn) {
        case 0x07:
            v_[x] = delay_timer_;
            break;
        case 0x0A:
            waiting_for_key_ = true;
            waiting_register_ = x;
            // Retry FX0A until a key is actually pressed.
            pc_ = static_cast<std::uint16_t>(pc_ - 2);
            break;
        case 0x15:
            delay_timer_ = v_[x];
            break;
        case 0x18:
            sound_timer_ = v_[x];
            break;
        case 0x1E:
            i_ = static_cast<std::uint16_t>(i_ + v_[x]);
            break;
        case 0x29: {
            const std::uint8_t digit = v_[x];
            if (digit > 0x0F) {
                setError("FX29 received a non-hex digit.");
                return false;
            }
            i_ = static_cast<std::uint16_t>(0x50 + digit * 5);
            break;
        }
        case 0x33:
            if (static_cast<std::size_t>(i_) + 2 >= MemorySize) {
                setError("FX33 moved outside RAM.");
                return false;
            }
            memory_[i_] = static_cast<std::uint8_t>(v_[x] / 100);
            memory_[i_ + 1] = static_cast<std::uint8_t>((v_[x] / 10) % 10);
            memory_[i_ + 2] = static_cast<std::uint8_t>(v_[x] % 10);
            break;
        case 0x55:
            if (i_ + x >= MemorySize) {
                setError("FX55 moved outside RAM.");
                return false;
            }
            for (std::uint8_t r = 0; r <= x; ++r) {
                memory_[i_ + r] = v_[r];
            }
            break;
        case 0x65:
            if (i_ + x >= MemorySize) {
                setError("FX65 moved outside RAM.");
                return false;
            }
            for (std::uint8_t r = 0; r <= x; ++r) {
                v_[r] = memory_[i_ + r];
            }
            break;
        default:
            setError("Unknown FXNN opcode.");
            return false;
        }
        break;

    default:
        setError("Unknown opcode.");
        return false;
    }

    return !halted_;
}

} // namespace chip8
