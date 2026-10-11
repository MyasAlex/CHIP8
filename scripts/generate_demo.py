"""Assemble the project's original two-player Pong and animated demo.
No third-party assembler or Python package is required.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class Assembler:
    def __init__(self):
        self.code = bytearray()
        self.labels = {}
        self.fixups = []

    def label(self, name):
        assert name not in self.labels
        self.labels[name] = 0x200 + len(self.code)

    def op(self, *ops):
        for op in ops:
            self.code.extend(op.to_bytes(2, "big"))

    def ref(self, base, label):
        self.fixups.append((len(self.code), base, label))
        self.op(base)

    def finish(self):
        for offset, base, name in self.fixups:
            value = self.labels[name]
            assert value <= 0xFFF
            self.code[offset:offset+2] = (base | value).to_bytes(2, "big")
        return bytes(self.code)


def pong():
    a = Assembler()
    a.op(0x00E0, 0x6002, 0x610D, 0x623D, 0x630D, 0x641F, 0x6510,
         0x6601, 0x6701, 0x6800, 0x6900)
    a.ref(0x2000, "objects")
    a.ref(0x2000, "scores")
    a.label("wait")
    a.op(0xFA07, 0x3A00)
    a.ref(0x1000, "wait")
    a.op(0x6A01, 0xFA15)
    a.ref(0x2000, "objects")  # erase previous positions
    for key, label in ((1, "left_up"), (4, "left_down"), (12, "right_up"), (13, "right_down")):
        a.op(0x6A00 | key, 0xEAA1)
        a.ref(0x2000, label)
    a.op(0x8464, 0x8574)
    for y in (6, 31):
        a.op(0x4500 | y)
        a.ref(0x2000, "bounce_y")
    a.op(0x4403)
    a.ref(0x2000, "hit_left")
    a.op(0x443C)
    a.ref(0x2000, "hit_right")
    a.op(0x4400)
    a.ref(0x2000, "point_right")
    a.op(0x443F)
    a.ref(0x2000, "point_left")
    a.ref(0x2000, "objects")
    a.ref(0x1000, "wait")
    for label, ops in (("left_up", (0x3106, 0x71FF)), ("left_down", (0x311B, 0x7101)),
                       ("right_up", (0x3306, 0x73FF)), ("right_down", (0x331B, 0x7301))):
        a.label(label); a.op(*ops, 0x00EE)
    a.label("bounce_y"); a.op(0x6A00, 0x87A7, 0x00EE)
    a.label("bounce_x"); a.op(0x6A00, 0x86A7, 0x6A02, 0xFA18, 0x00EE)
    for label, reg in (("hit_left", 1), ("hit_right", 3)):
        a.label(label)
        a.op(0x8A50, 0x8A05 | (reg << 4), 0x3F01)
        a.ref(0x1000, label+"_done")
        a.op(0x6B04, 0x8BA5, 0x3F00)
        a.ref(0x2000, "bounce_x")
        a.label(label+"_done"); a.op(0x00EE)
    for label, score, direction in (("point_right", 9, 1), ("point_left", 8, 255)):
        a.label(label)
        a.ref(0x2000, "scores")
        a.op(0x7001 | (score << 8), 0x6A0F, 0x8002 | (score << 8) | 0xA0)
        a.ref(0x2000, "scores")
        a.op(0x6600 | direction, 0x641F, 0x6510, 0xCA01, 0x6701, 0x3A00, 0x67FF, 0x6A05, 0xFA18, 0x00EE)
    a.label("objects")
    a.ref(0xA000, "paddle"); a.op(0xD015, 0xD235)
    a.ref(0xA000, "ball"); a.op(0xD451, 0x00EE)
    a.label("scores")
    a.op(0x6A14, 0x6B00, 0xF829, 0xDAB5, 0x6A28, 0xF929, 0xDAB5, 0x00EE)
    a.label("paddle"); a.code.extend([0x80]*5)
    a.label("ball"); a.code.append(0x80)
    return a.finish()


def demo():
    a = Assembler()
    a.op(0x00E0, 0x6012, 0x610B, 0x620C, 0xF229, 0xD015, 0x7020,
         0x6208, 0xF229, 0xD015, 0x6000, 0x611B, 0x6201)
    a.ref(0xA000, "dot"); a.op(0xD011)
    a.label("wait"); a.op(0xF307, 0x3300); a.ref(0x1000, "wait")
    a.op(0x6301, 0xF315, 0xD011, 0x8024, 0x403F); a.ref(0x2000, "turn")
    a.op(0x4000); a.ref(0x2000, "turn")
    a.op(0xD011); a.ref(0x1000, "wait")
    a.label("turn"); a.op(0x6300, 0x8237, 0x00EE)
    a.label("dot"); a.code.append(0xF0)
    return a.finish()


def emit(name, data):
    values = []
    for offset in range(0, len(data), 16):
        values.append("    " + ", ".join(f"0x{n:02X}" for n in data[offset:offset+16]) + ",")
    return f"inline const std::vector<std::uint8_t> {name} = {{\n" + "\n".join(values) + "\n};\n"


if __name__ == "__main__":
    original_pong = pong()
    original_demo = demo()
    (ROOT / "roms" / "Pong-2Players.ch8").write_bytes(original_pong)
    (ROOT / "roms" / "Studio-Demo.ch8").write_bytes(original_demo)
    content = "#pragma once\n#include <cstdint>\n#include <vector>\nnamespace chip8_demo {\n"
    content += emit("Demo", original_demo) + emit("Pong2", original_pong)
    content += emit("PongClassic", (ROOT / "roms" / "Pong.ch8").read_bytes())
    content += emit("Breakout", (ROOT / "roms" / "Breakout.ch8").read_bytes())
    content += "} // namespace chip8_demo\n"
    (ROOT / "src" / "DemoRom.hpp").write_text(content, encoding="utf-8")
    print(f"Original Pong: {len(original_pong)} bytes; demo: {len(original_demo)} bytes")
