"""Offline Timendus verification. Uses only Python's standard library.

Positive glyphs are checked at the coordinates specified by the test sources.
All 31 final framebuffers are compared with C-Octo reference captures.
"""
from pathlib import Path
import argparse
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
FLAG = [[1, 0, 1], [1, 1, 0], [1, 0, 0]]


def read_ppm(path):
    data = path.read_bytes()
    header = data.split(b"\n", 3)
    assert header[0] == b"P6" and header[2] == b"255", path
    width, height = map(int, header[1].split())
    pixels = header[3]
    assert len(pixels) == width * height * 3, path
    return width, height, pixels


def flag_at(image, x, y):
    width, height, pixels = image
    assert x + 3 <= width and y + 3 <= height
    got = []
    for j in range(3):
        row = []
        for i in range(3):
            offset = ((y+j)*width+x+i)*3
            row.append(int(pixels[offset:offset+3] != bytes([11, 20, 32])))
        got.append(row)
    return got == FLAG


def validate_glyphs(test, image):
    positions = []
    if test == "5-quirks":
        positions = [(59, y) for y in (2, 7, 12, 17, 22, 27)]
    elif test == "3-corax+":
        for x, ys in [(11, range(2, 28, 5)), (27, range(2, 28, 5)),
                      (43, range(2, 28, 5)), (59, range(2, 18, 5))]:
            positions.extend((x, y) for y in ys)
    elif test == "4-flags":
        for y, sets in [(1, [(27, 3), (49, 3)]), (6, [(5, 3), (27, 4), (49, 4)]),
                        (11, [(5, 3), (27, 4), (49, 3)]), (17, [(27, 4), (49, 4)]),
                        (22, [(5, 3), (27, 4), (49, 3)]), (28, [(31, 2)])]:
            for x, count in sets:
                positions.extend((x+n*4, y) for n in range(count))
    elif test == "6-keypad":
        positions = [(30, 9)]
    for x, y in positions:
        assert flag_at(image, x, y), f"{test}: failure glyph at {x},{y}"
    return len(positions)


def scenarios():
    for name, profile, choice in [("Modern", 1, 0), ("VIP", 0, 1), ("SCHIP", 2, 2), ("XO", 3, 3)]:
        for test in ("1-chip8-logo", "2-ibm-logo", "3-corax+", "4-flags", "5-quirks", "6-keypad", "7-beep"):
            if profile == 1 and test == "5-quirks":
                continue  # Modern is a convenient configurable combination, not a historical platform target.
            selected = choice if test == "5-quirks" else 3 if test == "6-keypad" else 0
            key = int(test == "6-keypad")
            frames = 1000 if test == "5-quirks" else 100 if key else 300
            yield name+"-"+test, profile, test, selected, frames, 1200 if profile == 0 else 700 if profile == 1 else 6000, key
        for selected in [1, 3] if profile == 2 else [4, 5] if profile == 3 else []:
            yield f"{name}-scroll-{selected}", profile, "8-scrolling", selected, 300, 6000, 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--runner", type=Path)
    parser.add_argument("--output", type=Path, default=ROOT / "build" / "rom-results")
    args = parser.parse_args()
    if args.runner:
        runner = args.runner.resolve()
    else:
        ext = ".exe" if sys.platform == "win32" else ""
        candidates = [ROOT/("CHIP8.RomRunner"+ext), ROOT/"build"/("CHIP8.RomRunner"+ext),
                      ROOT/"build"/"Release"/("CHIP8.RomRunner"+ext)]
        runner = next((p for p in candidates if p.is_file()), None)
        if runner is None:
            raise RuntimeError("Build CHIP8.RomRunner with CMake, or use the supplied Windows executable.")
    args.output.mkdir(parents=True, exist_ok=True)
    results = []
    for name, profile, test, selected, frames, hz, key in scenarios():
        source = ROOT / "third_party" / "timendus" / "bin" / (test+".ch8")
        prefix = (args.output/name).resolve()
        subprocess.run([str(runner), str(source), str(profile), str(selected), str(frames), str(hz), str(prefix), str(key)], check=True, stdout=subprocess.DEVNULL)
        image = read_ppm(prefix.with_suffix(".ppm"))
        checks = validate_glyphs(test, image)
        expected = read_ppm(ROOT / "tests" / "golden" / (name+".ppm"))
        assert image == expected, name+": framebuffer differs from C-Octo reference"
        stats = json.loads(prefix.with_suffix(".json").read_text())
        if test == "7-beep":
            assert 0 < stats["sound_frames"] < frames, name+": sound timer did not alternate"
        if key:
            assert stats["key_wait_frames"] >= 3, name+": FX0A did not block"
        results.append({"scenario": name, "passed": True, "positive_glyphs": checks, **stats})
        print(f"PASS {name}: {checks} positive glyphs; C-Octo framebuffer match")
    (args.output/"verification.json").write_text(json.dumps(results, indent=2)+"\n")
    print(f"All {len(results)} ROM scenarios passed.")


if __name__ == "__main__":
    main()
