"""Independently decode the real Win32 GIF and H264/AAC smoke recordings."""
from pathlib import Path
import array
import subprocess
import sys
import imageio_ffmpeg
from PIL import Image

root = Path(sys.argv[1])
with Image.open(root / "capture.gif") as image:
    assert image.size == (256, 128) and image.n_frames == 30
    duration = 0
    for index in range(image.n_frames):
        image.seek(index)
        image.load()
        duration += image.info["duration"]
    assert duration == 1000

ffmpeg = imageio_ffmpeg.get_ffmpeg_exe()
base = [ffmpeg, "-hide_banner", "-loglevel", "error", "-i", str(root / "capture.mp4")]
video = subprocess.run(base + ["-map", "0:v:0", "-fps_mode", "passthrough", "-pix_fmt", "rgb24", "-f", "rawvideo", "-"], check=True, capture_output=True).stdout
frame_size = 512*256*3
assert len(video) == frame_size*30, f"Video bytes/frames: {len(video)}"
last = Image.frombytes("RGB", (512, 256), video[-frame_size:])
with Image.open(root / "media-last.png") as expected:
    # A bright glyph in the top left catches RGB/BGR and bottom-up stride errors.
    a = last.crop((0, 0, 96, 64)).tobytes()
    b = expected.convert("RGB").crop((0, 0, 96, 64)).tobytes()
    mean_error = sum(abs(x-y) for x, y in zip(a, b))/len(a)
    assert mean_error < 10, f"MP4 orientation/color error: {mean_error}"
audio = subprocess.run(base + ["-map", "0:a:0", "-ac", "1", "-ar", "48000", "-f", "s16le", "-"], check=True, capture_output=True).stdout
samples = array.array("h")
samples.frombytes(audio)
assert 45000 <= len(samples) <= 52000, f"Audio duration/samples: {len(samples)}"
assert max(abs(value) for value in samples) > 500, "Beep was not encoded"
last.save(root / "decoded-mp4.png")
print("PASS native GIF: 30 frames/1 s; MP4: 512x256/30 frames, orientation/colors, decoded AAC beep")
