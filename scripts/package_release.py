"""Package a verified Windows build with only Python's standard library."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import struct
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ARCHIVE = "CHIP8-Studio-Windows-x64.zip"


def manifest():
    value = json.loads((ROOT / "version.json").read_text(encoding="utf-8"))
    version = value["version"]
    if not re.fullmatch(r"\d+\.\d+\.\d+", version) or value["tag"] != "v" + version:
        raise ValueError("Invalid release version/tag")
    parts = tuple(map(int, version.split(".")))
    cmake = (ROOT / "CMakeLists.txt").read_text()
    resources = (ROOT / "src/resource.rc").read_text()
    if not re.search(r"project\(CHIP8Studio VERSION " + re.escape(version.rsplit(".", 1)[0]) + r"\b", cmake):
        raise ValueError("CMake version differs from manifest")
    if f"FILEVERSION {parts[0]},{parts[1]},{parts[2]},0" not in resources:
        raise ValueError("Windows resource version differs from manifest")
    return value


def verify_pe(data):
    if data[:2] != b"MZ" or len(data) < 64:
        raise ValueError("Executable is not a Windows PE file")
    offset = struct.unpack_from("<I", data, 0x3C)[0]
    if offset + 96 > len(data) or data[offset:offset+4] != b"PE\0\0":
        raise ValueError("Invalid PE header")
    machine = struct.unpack_from("<H", data, offset + 4)[0]
    magic = struct.unpack_from("<H", data, offset + 24)[0]
    subsystem = struct.unpack_from("<H", data, offset + 24 + 68)[0]
    if (machine, magic, subsystem) != (0x8664, 0x20B, 2):
        raise ValueError("Release must be an x64 Windows GUI executable")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "dist")
    args = parser.parse_args()
    info = manifest()
    exe = args.exe.read_bytes()
    verify_pe(exe)
    for line in (ROOT / "roms/catalog/classics-files.sha256").read_text().splitlines():
        expected, name = line.split("  ", 1)
        original = ROOT / "roms/catalog/classics" / name
        if hashlib.sha256(original.read_bytes()).hexdigest() != expected:
            raise ValueError(f"Original classic collection bytes changed: {name}")
    for program in json.loads((ROOT / "roms/catalog/catalog.json").read_text(encoding="utf-8")):
        if hashlib.sha256((ROOT / program["path"]).read_bytes()).hexdigest() != program["sha256"]:
            raise ValueError(f"Catalog ROM bytes changed: {program['title']}")
    entries = {"CHIP8.Emulator.exe": exe}
    for source, destination in [
        ("START_HERE.txt", "START_HERE.txt"),
        ("START_HERE_EN.txt", "START_HERE_EN.txt"),
        ("LICENSE", "LICENSE"),
        ("THIRD_PARTY.md", "THIRD_PARTY.md"),
        ("roms/ORIGINAL_NOTICE.txt", "roms/ORIGINAL_NOTICE.txt"),
        ("docs/USER_GUIDE.md", "docs/USER_GUIDE.md"),
        ("docs/USER_GUIDE_RU.md", "docs/USER_GUIDE_RU.md"),
        ("docs/FIX_FLICKER_RU.md", "docs/FIX_FLICKER_RU.md"),
    ]:
        entries[destination] = (ROOT / source).read_bytes()
    for directory in [ROOT / "roms/catalog", ROOT / "third_party/chip8-database"]:
        for source in sorted(directory.rglob("*")):
            if source.is_file():
                entries[source.relative_to(ROOT).as_posix()] = source.read_bytes()
    entries["docs/STUDIO_3_RU.md"] = (ROOT / "docs/STUDIO_3_RU.md").read_bytes()
    hashes = "".join(f"{hashlib.sha256(data).hexdigest()}  {name}\n"
                     for name, data in sorted(entries.items()))
    entries["SHA256SUMS.txt"] = hashes.encode()
    args.output.mkdir(parents=True, exist_ok=True)
    archive = args.output / ARCHIVE
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as bundle:
        for name, data in sorted(entries.items()):
            item = zipfile.ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0))
            item.compress_type = zipfile.ZIP_DEFLATED
            item.external_attr = 0o100644 << 16
            bundle.writestr(item, data)
    with zipfile.ZipFile(archive) as bundle:
        if bundle.testzip() is not None or set(bundle.namelist()) != set(entries):
            raise RuntimeError("Archive verification failed")
        for name, data in entries.items():
            if bundle.read(name) != data:
                raise RuntimeError(f"Archive content differs: {name}")
    archive_hash = hashlib.sha256(archive.read_bytes()).hexdigest()
    (args.output / "SHA256SUMS.txt").write_text(f"{archive_hash}  {ARCHIVE}\n", encoding="utf-8")
    commit = os.environ.get("GITHUB_SHA")
    if not commit:
        result = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True)
        commit = result.stdout.strip() if result.returncode == 0 else "local"
    build_info = {
        "version": info["version"], "tag": info["tag"], "commit": commit,
        "platform": "windows-x64", "archive": ARCHIVE, "sha256": archive_hash,
        "executable_sha256": hashlib.sha256(exe).hexdigest(),
        "source_url": "https://github.com/MyasAlex/CHIP8",
    }
    (args.output / "build-info.json").write_text(json.dumps(build_info, indent=2) + "\n", encoding="utf-8")
    print(f"Verified {archive.name}: {archive.stat().st_size} bytes; SHA-256 {archive_hash}")


if __name__ == "__main__":
    main()
