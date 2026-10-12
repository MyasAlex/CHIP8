"""Decode the streaming GIF using an independent implementation (Pillow)."""
from pathlib import Path
from tempfile import TemporaryDirectory
import subprocess
import sys
from PIL import Image

with TemporaryDirectory() as directory:
    path = Path(directory) / "fixture.gif"
    subprocess.run([sys.argv[1], str(path)], check=True)
    with Image.open(path) as image:
        assert image.size == (256, 128) and image.n_frames == 12
        total = 0
        for frame in range(12):
            image.seek(frame)
            actual = image.convert("RGB").tobytes()
            expected = bytearray()
            for y in range(128):
                for x in range(256):
                    r = (x*7+y*11+frame*13) & 255
                    g = (x*19+y*3+frame*31) & 255
                    b = (x*23+y*29+frame*7) & 255
                    expected.extend(((r >> 5)*255//7, (g >> 5)*255//7, (b >> 6)*255//3))
            assert actual == expected, f"Decoded pixel mismatch in frame {frame}"
            total += image.info["duration"]
        assert total == 400
    print("PASS GIF LZW dictionary growth/reset, pixels, frame count and duration")
