"""Render captured core frames into documentation media. Requires Pillow."""
from pathlib import Path
import argparse
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
BG = "#090f18"
PANEL = "#0b1420"
TEXT = "#e6f1fb"
MUTED = "#91a5bc"
GREEN = "#75e3bd"


def font(size, bold=False):
    name = "DejaVuSans-Bold.ttf" if bold else "DejaVuSans.ttf"
    candidates = [
        Path("/usr/share/fonts/truetype/dejavu") / name,
        Path("C:/Windows/Fonts") / ("arialbd.ttf" if bold else "arial.ttf"),
    ]
    for path in candidates:
        if path.exists():
            return ImageFont.truetype(str(path), size)
    return ImageFont.load_default(size=size)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--frames", type=Path, required=True)
    args = parser.parse_args()
    media = ROOT / "docs/media"
    media.mkdir(parents=True, exist_ok=True)
    paths = sorted(args.frames.glob("frame-*.ppm"))
    if len(paths) != 180:
        raise RuntimeError("Expected all 180 captured core frames")
    frames = []
    for path in paths:
        game = Image.open(path).convert("RGB")
        if game.size != (64, 32):
            raise RuntimeError("Unexpected framebuffer dimensions")
        canvas = Image.new("RGB", (800, 480), BG)
        draw = ImageDraw.Draw(canvas)
        draw.text((32, 22), "CHIP-8 STUDIO", font=font(21, True), fill=TEXT)
        draw.rounded_rectangle((645, 17, 769, 49), radius=16, fill="#173128")
        draw.text((666, 23), "CORE / 60 Hz", font=font(13), fill=GREEN)
        draw.rounded_rectangle((29, 69, 771, 431), radius=12, outline="#25344b", width=2, fill=PANEL)
        canvas.paste(game.resize((704, 352), Image.Resampling.NEAREST), (48, 74))
        draw.text((32, 448), "PONG / TWO PLAYERS", font=font(14, True), fill=MUTED)
        draw.text((540, 448), "W / S     +     UP / DOWN", font=font(14), fill=GREEN)
        frames.append(canvas)
    # GIF stores timing in 10 ms units: 30/30/40 ms gives exactly 30 fps on average.
    frames[0].save(media/"gameplay.gif", save_all=True, append_images=frames[1:],
                   duration=[30,30,40]*60, loop=0, optimize=True, disposal=1)
    frames[50].save(media/"gameplay.png")
    social = Image.new("RGB", (1280, 640), BG)
    draw = ImageDraw.Draw(social)
    draw.rounded_rectangle((48, 46, 266, 84), radius=18, fill="#173128")
    draw.text((70, 55), "FREE / OPEN SOURCE", font=font(16, True), fill=GREEN)
    draw.text((48, 127), "CHIP-8", font=font(72, True), fill=TEXT)
    draw.text((48, 204), "STUDIO", font=font(72, True), fill=TEXT)
    draw.text((52, 321), "Play. Pause. Rewind.", font=font(27), fill=GREEN)
    draw.text((52, 377), "Portable emulator for Windows", font=font(21), fill=MUTED)
    draw.text((52, 417), "CHIP-8 / Super-CHIP / XO-CHIP", font=font(18), fill=MUTED)
    draw.rounded_rectangle((52, 512, 471, 569), radius=12, fill=GREEN)
    draw.text((74, 528), "github.com/MyasAlex/CHIP8", font=font(22, True), fill=BG)
    draw.rounded_rectangle((627, 145, 1240, 490), radius=18, fill=PANEL, outline="#25344b", width=2)
    game = Image.open(paths[50]).convert("RGB").resize((576, 288), Image.Resampling.NEAREST)
    social.paste(game, (646, 174))
    draw.text((642, 517), "Actual emulator core gameplay", font=font(18), fill=MUTED)
    social.save(media/"social-preview.png")
    print("Rendered gameplay GIF, still image and social preview.")


if __name__ == "__main__":
    main()
