#include "Chip8Core.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <utility>

namespace chip8 {
namespace {
constexpr std::array<std::uint8_t, 80> Font = {
    0xF0,0x90,0x90,0x90,0xF0, 0x20,0x60,0x20,0x20,0x70,
    0xF0,0x10,0xF0,0x80,0xF0, 0xF0,0x10,0xF0,0x10,0xF0,
    0x90,0x90,0xF0,0x10,0x10, 0xF0,0x80,0xF0,0x10,0xF0,
    0xF0,0x80,0xF0,0x90,0xF0, 0xF0,0x10,0x20,0x40,0x40,
    0xF0,0x90,0xF0,0x90,0xF0, 0xF0,0x90,0xF0,0x10,0xF0,
    0xF0,0x90,0xF0,0x90,0x90, 0xE0,0x90,0xE0,0x90,0xE0,
    0xF0,0x80,0x80,0x80,0xF0, 0xE0,0x90,0x90,0x90,0xE0,
    0xF0,0x80,0xF0,0x80,0xF0, 0xF0,0x80,0xF0,0x80,0x80
};
std::string hex(std::uint32_t n, int width = 4) {
    std::ostringstream s;
    s << std::uppercase << std::hex << std::setfill('0') << std::setw(width) << n;
    return s.str();
}
std::uint32_t crc32(const std::uint8_t* bytes, std::size_t size) {
    std::uint32_t crc = ~0u;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (int b = 0; b < 8; ++b) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
}

Config Config::preset(Profile p) {
    Config c;
    c.profile = p;
    if (p == Profile::Vip) {
        c.shiftUsesVx = false;
        c.incrementI = true;
        c.logicResetsVf = true;
        c.clipSprites = true;
        c.drawWait = true;
    } else if (p == Profile::SuperChip) {
        c.jumpUsesVx = true;
        c.clipSprites = true;
    } else if (p == Profile::XoChip) {
        c.shiftUsesVx = false;
        c.incrementI = true;
    }
    return c;
}
std::uint8_t Config::quirks() const {
    return static_cast<std::uint8_t>(shiftUsesVx | (incrementI << 1) | (logicResetsVf << 2) |
        (jumpUsesVx << 3) | (clipSprites << 4) | (drawWait << 5) | (keyRelease << 6));
}
Config Config::fromQuirks(Profile p, std::uint8_t bits) {
    Config c;
    c.profile = p;
    c.shiftUsesVx = (bits & 1) != 0;
    c.incrementI = (bits & 2) != 0;
    c.logicResetsVf = (bits & 4) != 0;
    c.jumpUsesVx = (bits & 8) != 0;
    c.clipSprites = (bits & 16) != 0;
    c.drawWait = (bits & 32) != 0;
    c.keyRelease = (bits & 64) != 0;
    return c;
}
const char* profileName(Profile p) {
    switch (p) {
    case Profile::Vip: return "CHIP-8 (VIP)";
    case Profile::Modern: return "CHIP-8 (Modern)";
    case Profile::SuperChip: return "Super-CHIP (Modern)";
    case Profile::XoChip: return "XO-CHIP 1.1";
    }
    return "Unknown";
}

Chip8::Chip8(Config c) : config_(c) { reset(); }
void Chip8::reset() {
    memory_.fill(0); v_.fill(0); stack_.fill(0); display_.fill(0); keys_.fill(false);
    audio_.fill(0);
    // Large 8x10 font is an original 2x enlargement of the standard 4x5 font.
    std::copy(Font.begin(), Font.end(), memory_.begin() + 0x50);
    for (std::size_t glyph = 0; glyph < 16; ++glyph) {
        for (std::size_t row = 0; row < 5; ++row) {
            std::uint8_t expanded = 0;
            for (int bit = 0; bit < 4; ++bit)
                if (Font[glyph * 5 + row] & (0x80 >> bit))
                    expanded |= static_cast<std::uint8_t>(0xC0 >> (bit * 2));
            memory_[0xA0 + glyph * 10 + row * 2] = expanded;
            memory_[0xA0 + glyph * 10 + row * 2 + 1] = expanded;
        }
    }
    i_ = 0; pc_ = ProgramStart; sp_ = dt_ = st_ = 0; plane_ = 1; pitch_ = 64;
    hires_ = halted_ = waiting_key_ = waiting_draw_ = draw_ready_ = false;
    key_reg_ = 0; pending_key_ = -1;
    rng_ = 0xC0FFEE12; instructions_ = rom_hash_ = rom_size_ = 0;
    error_.clear(); changed();
    // flags_ deliberately survive a warm reset, like SCHIP RPL user flags.
}
bool Chip8::loadRom(const std::vector<std::uint8_t>& bytes) { return loadRom(bytes.data(), bytes.size()); }
bool Chip8::loadRom(const std::uint8_t* bytes, std::size_t size) {
    if (!bytes || size < 2 || size > memoryLimit() - ProgramStart) return false;
    reset();
    std::copy_n(bytes, size, memory_.begin() + ProgramStart);
    rom_hash_ = 14695981039346656037ull;
    for (std::size_t i = 0; i < size; ++i) { rom_hash_ ^= bytes[i]; rom_hash_ *= 1099511628211ull; }
    rom_size_ = size;
    return true;
}
void Chip8::seedRandom(std::uint32_t seed) { rng_ = seed ? seed : 0xC0FFEE12; }
std::uint8_t Chip8::randomByte() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
    return static_cast<std::uint8_t>(rng_);
}
bool Chip8::range(std::size_t address, std::size_t count) const {
    return address < memoryLimit() && count <= memoryLimit() - address;
}
bool Chip8::fail(const char* message, std::uint16_t address, std::uint16_t opcode) {
    error_ = std::string(message) + " [PC=" + hex(address) + ", opcode=" + hex(opcode) + "]";
    halted_ = true;
    return false;
}
void Chip8::changed(DisplayUpdate update) { ++generation_; display_update_ = update; }
void Chip8::clearDisplay() {
    for (auto& pixel : display_) pixel &= static_cast<std::uint8_t>(~plane_);
    changed();
}
void Chip8::scroll(int dx, int dy) {
    const auto old = display_;
    const int w = width(), h = height();
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        const int sx = x - dx, sy = y - dy;
        auto& pixel = display_[static_cast<std::size_t>(y) * DisplayWidth + x];
        pixel &= static_cast<std::uint8_t>(~plane_);
        if (sx >= 0 && sx < w && sy >= 0 && sy < h)
            pixel |= static_cast<std::uint8_t>(old[static_cast<std::size_t>(sy) * DisplayWidth + sx] & plane_);
    }
    changed();
}
bool Chip8::draw(std::uint8_t vx, std::uint8_t vy, std::uint8_t rows,
                 std::uint16_t address, std::uint16_t opcode) {
    const int w = width(), h = height();
    const int spriteW = rows ? 8 : 16, spriteH = rows ? rows : 16;
    const int bytesPerPlane = rows ? rows : 32;
    const int planes = (plane_ & 1 ? 1 : 0) + (plane_ & 2 ? 1 : 0);
    if (!rows && !config_.extended()) { v_[15] = 0; return true; }
    if (planes && !range(i_, static_cast<std::size_t>(bytesPerPlane * planes)))
        return fail("Sprite read outside RAM", address, opcode);
    const int startX = vx % w, startY = vy % h;
    sprite_update_ = {};
    sprite_update_.opcode = opcode;
    sprite_update_.x = static_cast<std::uint8_t>(startX);
    sprite_update_.y = static_cast<std::uint8_t>(startY);
    sprite_update_.width = static_cast<std::uint8_t>(spriteW);
    sprite_update_.rows = static_cast<std::uint8_t>(spriteH);
    sprite_update_.planes = plane_;
    sprite_update_.clipped = config_.clipSprites;
    if (planes) std::copy_n(memory_.begin() + i_, bytesPerPlane * planes, sprite_update_.bytes.begin());
    int dataOffset = 0;
    bool collision = false, added = false;
    for (int mask = 1; mask <= 2; mask <<= 1) {
        if (!(plane_ & mask)) continue;
        for (int row = 0; row < spriteH; ++row) {
            for (int bit = 0; bit < spriteW; ++bit) {
                const auto data = memory_[i_ + dataOffset + row * (spriteW / 8) + bit / 8];
                if (!(data & (0x80 >> (bit & 7)))) continue;
                const int px = startX + bit, py = startY + row;
                if (config_.clipSprites && (px >= w || py >= h)) continue;
                auto& pixel = display_[static_cast<std::size_t>(py % h) * DisplayWidth + px % w];
                if (pixel & mask) collision = true; else added = true;
                pixel ^= static_cast<std::uint8_t>(mask);
            }
        }
        dataOffset += bytesPerPlane;
    }
    v_[15] = collision ? 1 : 0; // coordinates were captured before touching VF
    changed(added ? DisplayUpdate::DrawAdd : DisplayUpdate::DrawErase);
    return true;
}
bool Chip8::skip(std::uint16_t address, std::uint16_t opcode) {
    if (!range(pc_, 2)) return fail("Skip fetch outside RAM", address, opcode);
    const int length = config_.xo() && memory_[pc_] == 0xF0 && memory_[pc_ + 1] == 0 ? 4 : 2;
    if (!range(pc_, length)) return fail("Truncated skipped instruction", address, opcode);
    pc_ = static_cast<std::uint16_t>(pc_ + length);
    return true;
}
void Chip8::tickTimers() {
    if (dt_) --dt_;
    if (st_) --st_;
    draw_ready_ = true;
    waiting_draw_ = false;
}
bool Chip8::isKeyPressed(std::uint8_t key) const { return key < KeyCount && keys_[key]; }
void Chip8::releaseKeys() {
    keys_.fill(false);
    pending_key_ = -1;
}
void Chip8::setKey(std::uint8_t key, bool pressed) {
    if (key >= KeyCount || keys_[key] == pressed) return;
    keys_[key] = pressed;
    if (!waiting_key_) return;
    if (pressed && pending_key_ < 0) {
        pending_key_ = key;
        if (!config_.keyRelease) { v_[key_reg_] = key; waiting_key_ = false; pending_key_ = -1; }
    } else if (!pressed && pending_key_ == key) {
        v_[key_reg_] = key;
        waiting_key_ = false;
        pending_key_ = -1;
    }
}
bool Chip8::poke(std::uint16_t address, std::uint8_t value) {
    if (!range(address, 1)) return false;
    memory_[address] = value;
    return true;
}

bool Chip8::step() {
    if (halted_) return false;
    if (waiting_key_) return true;
    const std::uint16_t address = pc_;
    if (!range(pc_, 2)) return fail("Instruction fetch outside RAM", address, 0);
    const std::uint16_t op = static_cast<std::uint16_t>((memory_[pc_] << 8) | memory_[pc_ + 1]);
    if ((op & 0xF000) == 0xD000 && config_.drawWait && !draw_ready_) {
        waiting_draw_ = true;
        return true;
    }
    pc_ = static_cast<std::uint16_t>(pc_ + 2);
    ++instructions_;
    const std::uint8_t x = (op >> 8) & 15, y = (op >> 4) & 15, n = op & 15;
    const auto nn = static_cast<std::uint8_t>(op);
    const std::uint16_t nnn = op & 0xFFF;
    const auto vx = v_[x], vy = v_[y]; // snapshot both operands, including VF
    switch (op & 0xF000) {
    case 0x0000:
        if (op == 0x00E0) clearDisplay();
        else if (op == 0x00EE) {
            if (!sp_) return fail("Stack underflow", address, op);
            pc_ = stack_[--sp_];
        } else if (config_.extended() && op == 0x00FD) halted_ = true;
        else if (config_.extended() && (op == 0x00FE || op == 0x00FF)) {
            hires_ = op == 0x00FF; display_.fill(0); changed();
        } else if (config_.extended() && op == 0x00FB) scroll(4, 0);
        else if (config_.extended() && op == 0x00FC) scroll(-4, 0);
        else if (config_.extended() && (op & 0xFFF0) == 0x00C0) scroll(0, n);
        else if (config_.xo() && (op & 0xFFF0) == 0x00D0) scroll(0, -static_cast<int>(n));
        else if (config_.extended()) return fail("Unsupported system instruction", address, op);
        // VIP/modern 0NNN is a native RCA call; an interpreter cannot execute it.
        // Ignore it for classic compatibility. Extended profiles reject it.
        break;
    case 0x1000: pc_ = nnn; break;
    case 0x2000:
        if (sp_ == StackSize) return fail("Stack overflow", address, op);
        stack_[sp_++] = pc_; pc_ = nnn; break;
    case 0x3000: if (vx == nn) return skip(address, op); break;
    case 0x4000: if (vx != nn) return skip(address, op); break;
    case 0x5000:
        if (n == 0) { if (vx == vy) return skip(address, op); }
        else if (config_.xo() && (n == 2 || n == 3)) {
            const int count = std::abs(static_cast<int>(x) - y) + 1;
            if (!range(i_, count)) return fail("Ranged transfer outside RAM", address, op);
            for (int z = 0; z < count; ++z) {
                const int reg = x <= y ? x + z : x - z;
                if (n == 2) memory_[i_ + z] = v_[reg]; else v_[reg] = memory_[i_ + z];
            }
        } else return fail("Unknown 5XYN instruction", address, op);
        break;
    case 0x6000: v_[x] = nn; break;
    case 0x7000: v_[x] = static_cast<std::uint8_t>(vx + nn); break;
    case 0x8000: {
        std::uint8_t result = 0, flag = 0;
        bool writesFlag = true;
        switch (n) {
        case 0: v_[x] = vy; writesFlag = false; break;
        case 1: v_[x] = vx | vy; writesFlag = config_.logicResetsVf; break;
        case 2: v_[x] = vx & vy; writesFlag = config_.logicResetsVf; break;
        case 3: v_[x] = vx ^ vy; writesFlag = config_.logicResetsVf; break;
        case 4: result = static_cast<std::uint8_t>(vx + vy); flag = (vx + vy) > 255; v_[x] = result; break;
        case 5: result = static_cast<std::uint8_t>(vx - vy); flag = vx >= vy; v_[x] = result; break;
        case 7: result = static_cast<std::uint8_t>(vy - vx); flag = vy >= vx; v_[x] = result; break;
        case 6: result = config_.shiftUsesVx ? vx : vy; flag = result & 1; v_[x] = result >> 1; break;
        case 14: result = config_.shiftUsesVx ? vx : vy; flag = result >> 7; v_[x] = static_cast<std::uint8_t>(result << 1); break;
        default: return fail("Unknown arithmetic instruction", address, op);
        }
        if (writesFlag) v_[15] = flag; // result first, VF last (x/y may be F)
        break;
    }
    case 0x9000:
        if (n) return fail("Unknown 9XYN instruction", address, op);
        if (vx != vy) return skip(address, op);
        break;
    case 0xA000: i_ = nnn; break;
    case 0xB000: pc_ = static_cast<std::uint16_t>(nnn + v_[config_.jumpUsesVx ? x : 0]); break;
    case 0xC000: v_[x] = randomByte() & nn; break;
    case 0xD000:
        draw_ready_ = false; waiting_draw_ = false;
        return draw(vx, vy, n, address, op);
    case 0xE000:
        if (nn == 0x9E) { if (isKeyPressed(vx)) return skip(address, op); }
        else if (nn == 0xA1) { if (!isKeyPressed(vx)) return skip(address, op); }
        else return fail("Unknown keyboard instruction", address, op);
        break;
    case 0xF000:
        if (op == 0xF000 && config_.xo()) {
            if (!range(pc_, 2)) return fail("Truncated long index instruction", address, op);
            i_ = static_cast<std::uint16_t>((memory_[pc_] << 8) | memory_[pc_ + 1]);
            pc_ = static_cast<std::uint16_t>(pc_ + 2);
        } else if (nn == 1 && config_.xo() && x < 4) plane_ = x;
        else if (op == 0xF002 && config_.xo()) {
            if (!range(i_, 16)) return fail("Audio read outside RAM", address, op);
            std::copy_n(memory_.begin() + i_, 16, audio_.begin());
        } else switch (nn) {
        case 7: v_[x] = dt_; break;
        case 0x0A:
            waiting_key_ = true; key_reg_ = x; pending_key_ = -1;
            for (int key = 0; key < 16; ++key) if (keys_[key]) {
                pending_key_ = key;
                if (!config_.keyRelease) { v_[x] = static_cast<std::uint8_t>(key); waiting_key_ = false; }
                break;
            }
            break;
        case 0x15: dt_ = vx; break;
        case 0x18: st_ = vx; break;
        case 0x1E: i_ = static_cast<std::uint16_t>(i_ + vx); break;
        case 0x29: i_ = static_cast<std::uint16_t>(0x50 + (vx & 15) * 5); break;
        case 0x30:
            if (!config_.extended()) return fail("Large font requires Super-CHIP", address, op);
            i_ = static_cast<std::uint16_t>(0xA0 + (vx & 15) * 10); break;
        case 0x33:
            if (!range(i_, 3)) return fail("BCD write outside RAM", address, op);
            memory_[i_] = vx / 100; memory_[i_ + 1] = (vx / 10) % 10; memory_[i_ + 2] = vx % 10;
            break;
        case 0x55: case 0x65:
            if (!range(i_, static_cast<std::size_t>(x) + 1)) return fail("Register transfer outside RAM", address, op);
            for (int reg = 0; reg <= x; ++reg)
                if (nn == 0x55) memory_[i_ + reg] = v_[reg]; else v_[reg] = memory_[i_ + reg];
            if (config_.incrementI) i_ = static_cast<std::uint16_t>(i_ + x + 1);
            break;
        case 0x75: case 0x85:
            if (!config_.extended() || (!config_.xo() && x > 7)) return fail("Invalid RPL flags range", address, op);
            for (int reg = 0; reg <= x; ++reg) if (nn == 0x75) flags_[reg] = v_[reg]; else v_[reg] = flags_[reg];
            break;
        case 0x3A:
            if (!config_.xo()) return fail("Audio pitch requires XO-CHIP", address, op);
            pitch_ = vx; break;
        default: return fail("Unknown miscellaneous instruction", address, op);
        }
        break;
    default: return fail("Unknown instruction", address, op);
    }
    return !halted_;
}

std::string Chip8::disassemble(std::uint16_t address) const {
    if (!range(address, 2)) return "<outside RAM>";
    const auto op = static_cast<std::uint16_t>((memory_[address] << 8) | memory_[address + 1]);
    const auto x = (op >> 8) & 15, y = (op >> 4) & 15, n = op & 15;
    const std::string vx = "V" + hex(x, 1), vy = "V" + hex(y, 1);
    std::string text;
    if (op == 0x00E0) text = "CLS";
    else if (op == 0x00EE) text = "RET";
    else if (op == 0x00FD) text = "EXIT";
    else if (op == 0x00FE) text = "LORES";
    else if (op == 0x00FF) text = "HIRES";
    else if (op == 0x00FB) text = "SCROLL RIGHT 4";
    else if (op == 0x00FC) text = "SCROLL LEFT 4";
    else if ((op & 0xFFF0) == 0x00C0) text = "SCROLL DOWN " + hex(n, 1);
    else if ((op & 0xFFF0) == 0x00D0) text = "SCROLL UP " + hex(n, 1);
    else if (op == 0xF000 && config_.xo() && range(address, 4))
        text = "LD I, LONG " + hex((memory_[address + 2] << 8) | memory_[address + 3]);
    else switch (op >> 12) {
    case 0: text = "SYS " + hex(op & 0xFFF, 3); break;
    case 1: text = "JP " + hex(op & 0xFFF, 3); break;
    case 2: text = "CALL " + hex(op & 0xFFF, 3); break;
    case 3: text = "SE " + vx + ", " + hex(op & 255, 2); break;
    case 4: text = "SNE " + vx + ", " + hex(op & 255, 2); break;
    case 5: text = n == 0 ? "SE " + vx + ", " + vy : (n == 2 ? "SAVE " : "LOAD ") + vx + "-" + vy; break;
    case 6: text = "LD " + vx + ", " + hex(op & 255, 2); break;
    case 7: text = "ADD " + vx + ", " + hex(op & 255, 2); break;
    case 8: {
        const char* names[16] = {"LD","OR","AND","XOR","ADD","SUB","SHR","SUBN","?","?","?","?","?","?","SHL","?"};
        text = std::string(names[n]) + " " + vx + ", " + vy; break;
    }
    case 9: text = "SNE " + vx + ", " + vy; break;
    case 10: text = "LD I, " + hex(op & 0xFFF, 3); break;
    case 11: text = "JP V" + hex(config_.jumpUsesVx ? x : 0, 1) + ", " + hex(op & 0xFFF, 3); break;
    case 12: text = "RND " + vx + ", " + hex(op & 255, 2); break;
    case 13: text = "DRW " + vx + ", " + vy + ", " + hex(n, 1); break;
    case 14: text = std::string((op & 255) == 0x9E ? "SKP " : "SKNP ") + vx; break;
    case 15:
        switch (op & 255) {
        case 1: text = "PLANE " + hex(x, 1); break;
        case 2: text = "AUDIO"; break;
        case 7: text = "LD " + vx + ", DT"; break;
        case 10: text = "WAIT KEY " + vx; break;
        case 0x15: text = "LD DT, " + vx; break;
        case 0x18: text = "LD ST, " + vx; break;
        case 0x1E: text = "ADD I, " + vx; break;
        case 0x29: text = "FONT " + vx; break;
        case 0x30: text = "BIG FONT " + vx; break;
        case 0x33: text = "BCD " + vx; break;
        case 0x55: text = "SAVE V0-" + vx; break;
        case 0x65: text = "LOAD V0-" + vx; break;
        case 0x75: text = "SAVE FLAGS " + vx; break;
        case 0x85: text = "LOAD FLAGS " + vx; break;
        case 0x3A: text = "PITCH " + vx; break;
        default: text = "?";
        }
        break;
    }
    return hex(address) + "  " + hex(op) + "  " + text;
}

std::vector<std::uint8_t> Chip8::saveState() const {
    std::vector<std::uint8_t> out{'C','8','S','T','A','T','E',2};
    auto put = [&out](std::uint64_t value, int bytes) {
        for (int i = 0; i < bytes; ++i) out.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
    };
    put(rom_hash_,8); put(rom_size_,4);
    put(static_cast<std::uint8_t>(config_.profile),1); put(config_.quirks(),1);
    put(i_,2); put(pc_,2); put(sp_,1); put(dt_,1); put(st_,1); put(plane_,1); put(pitch_,1);
    put(hires_,1); put(halted_,1); put(waiting_key_,1); put(draw_ready_,1); put(waiting_draw_,1);
    put(key_reg_,1); put(pending_key_ + 1,1); put(rng_,4); put(instructions_,8);
    for (const auto& array : {&v_, &flags_, &audio_}) out.insert(out.end(), array->begin(), array->end());
    for (auto address : stack_) put(address,2);
    out.insert(out.end(), memory_.begin(), memory_.end());
    out.insert(out.end(), display_.begin(), display_.end());
    const auto errorSize = std::min<std::size_t>(error_.size(),512);
    put(errorSize,2); out.insert(out.end(),error_.begin(),error_.begin()+errorSize);
    put(crc32(out.data(),out.size()),4);
    return out;
}
bool Chip8::loadState(const std::vector<std::uint8_t>& data, std::string& error) {
    error.clear();
    auto invalid = [&error](const char* message) { error = message; return false; };
    if (data.size() < MemorySize + DisplaySize || data.size() > MemorySize + DisplaySize + 1024)
        return invalid("Invalid state size");
    const std::array<std::uint8_t,8> magic{'C','8','S','T','A','T','E',2};
    if (!std::equal(magic.begin(),magic.end(),data.begin())) return invalid("Unknown state version");
    const std::size_t end = data.size() - 4;
    std::uint32_t expected = 0;
    for (int n = 0; n < 4; ++n) expected |= static_cast<std::uint32_t>(data[end+n]) << (8*n);
    if (crc32(data.data(),end) != expected) return invalid("State checksum mismatch");
    std::size_t pos = 8;
    bool ok = true;
    auto get = [&](int count) {
        std::uint64_t value = 0;
        if (pos + count > end) { ok = false; return value; }
        for (int n = 0; n < count; ++n) value |= static_cast<std::uint64_t>(data[pos++]) << (8*n);
        return value;
    };
    Chip8 candidate;
    candidate.rom_hash_ = get(8); candidate.rom_size_ = static_cast<std::size_t>(get(4));
    if (!rom_hash_ || candidate.rom_hash_ != rom_hash_ || candidate.rom_size_ != rom_size_)
        return invalid("State belongs to another ROM");
    const auto profile = get(1), quirks = get(1);
    if (profile > 3 || quirks > 127) return invalid("Invalid compatibility settings");
    candidate.config_ = Config::fromQuirks(static_cast<Profile>(profile),static_cast<std::uint8_t>(quirks));
    candidate.i_ = static_cast<std::uint16_t>(get(2)); candidate.pc_ = static_cast<std::uint16_t>(get(2));
    candidate.sp_ = static_cast<std::uint8_t>(get(1)); candidate.dt_ = static_cast<std::uint8_t>(get(1));
    candidate.st_ = static_cast<std::uint8_t>(get(1)); candidate.plane_ = static_cast<std::uint8_t>(get(1));
    candidate.pitch_ = static_cast<std::uint8_t>(get(1));
    auto boolean = [&]() { const auto b=get(1); if(b>1)ok=false; return b!=0; };
    candidate.hires_=boolean(); candidate.halted_=boolean(); candidate.waiting_key_=boolean();
    candidate.draw_ready_=boolean(); candidate.waiting_draw_=boolean();
    candidate.key_reg_=static_cast<std::uint8_t>(get(1)); candidate.pending_key_=static_cast<int>(get(1))-1;
    candidate.rng_=static_cast<std::uint32_t>(get(4)); candidate.instructions_=get(8);
    for (auto* array : {&candidate.v_, &candidate.flags_, &candidate.audio_}) for(auto& b:*array)b=static_cast<std::uint8_t>(get(1));
    for(auto& address:candidate.stack_)address=static_cast<std::uint16_t>(get(2));
    for(auto& b:candidate.memory_)b=static_cast<std::uint8_t>(get(1));
    for(auto& b:candidate.display_)b=static_cast<std::uint8_t>(get(1));
    const auto errorSize=get(2);
    if(!ok || errorSize>512 || pos+errorSize!=end)return invalid("Truncated state");
    candidate.error_.assign(data.begin()+pos,data.begin()+pos+errorSize);
    if(candidate.sp_>StackSize || candidate.plane_>3 || candidate.key_reg_>15 || candidate.pending_key_>15 || !candidate.rng_)
        return invalid("Invalid CPU state");
    if(!candidate.config_.xo() && candidate.plane_!=1)return invalid("Planes require XO-CHIP");
    if(!candidate.config_.extended() && candidate.hires_)return invalid("Resolution requires Super-CHIP");
    if(candidate.rom_size_>candidate.memoryLimit()-ProgramStart)return invalid("ROM exceeds profile memory");
    if(!candidate.halted_ && !candidate.range(candidate.pc_,2))return invalid("Invalid program counter");
    if(std::any_of(candidate.display_.begin(),candidate.display_.end(),[&](auto b){return b>(candidate.config_.xo()?3:1);}))
        return invalid("Invalid framebuffer");
    candidate.releaseKeys(); // host keys cannot safely survive a restore
    candidate.generation_=generation_+1; candidate.display_update_=DisplayUpdate::Other;
    *this=std::move(candidate);
    return true;
}

void Scheduler::setHz(double hz) {
    hz_ = std::isfinite(hz) ? std::clamp(hz,60.0,100000.0) : 700.0;
    cpu_left_=1.0/hz_;
}
void Scheduler::reset() { cpu_left_=1.0/hz_; frame_left_=1.0/60.0; }
double Scheduler::untilNextEvent() const { return std::min(cpu_left_,frame_left_); }
void Scheduler::advance(Chip8& cpu, double seconds, const std::function<void()>& onFrame,
                        const std::function<void()>& onDisplay) {
    if(!std::isfinite(seconds) || seconds<=0)return;
    seconds=std::min(seconds,0.05); // discard suspended-host catch-up beyond 50 ms
    constexpr double epsilon=1e-12;
    while(seconds>epsilon) {
        const double event=std::min(cpu_left_,frame_left_);
        if(event>seconds+epsilon) { cpu_left_-=seconds; frame_left_-=seconds; break; }
        cpu_left_-=event; frame_left_-=event; seconds-=event;
        const bool frame=frame_left_<=epsilon;
        if(frame) { if(!cpu.halted())cpu.tickTimers(); frame_left_+=1.0/60.0; }
        if(cpu_left_<=epsilon) {
            if(!cpu.halted()) {
                const auto generation=cpu.displayGeneration();
                cpu.step();
                if(onDisplay && generation!=cpu.displayGeneration())onDisplay();
            }
            cpu_left_+=1.0/hz_;
        }
        if(frame && onFrame)onFrame();
    }
}
void Presenter::observe(const Chip8& cpu) {
    const auto generation=cpu.displayGeneration();
    if(generation_==generation)return;
    const bool contiguous=generation_!=~std::uint64_t{0} && generation==generation_+1;
    generation_=generation;
    // Clear/scroll/mode changes and restoration have no sprite pairing.
    // Also discard old holds when the caller has missed display instructions.
    if(!contiguous || cpu.displayUpdate()==DisplayUpdate::Other) {
        pending_.clear();
        return;
    }
    const auto& sprite=cpu.spriteUpdate();
    const auto key=static_cast<std::uint16_t>(sprite.opcode & 0x0FFF);
    if(cpu.displayUpdate()==DisplayUpdate::DrawAdd) {
        // Match by coordinate registers, dimensions and selected planes, not
        // instruction address or sprite bytes: erase/redraw sites may differ
        // and an animated sprite may change its shape. Prefer the nearest
        // previous position when several objects share the registers; FIFO
        // breaks ties. This also handles score digits redrawn out of order.
        auto found=pending_.end();
        int nearest=cpu.width()+cpu.height()+1;
        for(auto it=pending_.begin();it!=pending_.end();++it) {
            if(it->key!=key || it->planes!=sprite.planes)continue;
            int dx=std::abs(static_cast<int>(it->x)-sprite.x);
            int dy=std::abs(static_cast<int>(it->y)-sprite.y);
            if(!sprite.clipped) {
                dx=std::min(dx,cpu.width()-dx);dy=std::min(dy,cpu.height()-dy);
            }
            if(dx+dy<nearest){nearest=dx+dy;found=it;}
        }
        if(found!=pending_.end())pending_.erase(found);
        return;
    }
    PendingSprite held{};
    held.key=key;held.planes=sprite.planes;held.x=sprite.x;held.y=sprite.y;
    const int w=cpu.width(),h=cpu.height(),bytesPerRow=sprite.width/8;
    int offset=0;
    for(int plane=1;plane<=2;plane<<=1) {
        if(!(sprite.planes & plane))continue;
        for(int row=0;row<sprite.rows;++row)for(int bit=0;bit<sprite.width;++bit) {
            if(!(sprite.bytes[offset+row*bytesPerRow+bit/8] & (0x80>>(bit&7))))continue;
            const int x=sprite.x+bit,y=sprite.y+row;
            if(sprite.clipped && (x>=w || y>=h))continue;
            held.pixels.push_back({static_cast<std::uint16_t>((y%h)*Chip8::DisplayWidth+x%w),
                                   static_cast<std::uint8_t>(plane)});
        }
        offset+=sprite.rows*bytesPerRow;
    }
    if(held.pixels.empty())return;
    if(pending_.size()==64)pending_.erase(pending_.begin());
    pending_.push_back(std::move(held));
}
bool Presenter::publish(const Chip8& cpu, bool stabilize, bool force) {
    const bool first=generation_==~std::uint64_t{0};
    observe(cpu);
    auto next=cpu.display();
    if(!stabilize || force)pending_.clear();
    else {
        for(auto& held:pending_)++held.age;
        std::erase_if(pending_,[](const auto& held){return held.age>=4;});
        for(const auto& held:pending_)for(const auto& pixel:held.pixels)
            next[pixel.index]|=pixel.plane;
    }
    const bool changed=first || force || width_!=cpu.width() || height_!=cpu.height() || next!=display_;
    display_=next; width_=cpu.width(); height_=cpu.height();
    return changed;
}
} // namespace chip8
