# Media and provenance

- banner.svg is a code-authored vector banner.
- gameplay.gif and gameplay.png show actual framebuffer output of this emulator's core running the project's original two-player Pong. Scripted inputs move both paddles for the demonstration. They are not recordings of the desktop window and do not demonstrate a built-in bot.
- social-preview.png is a promotional card incorporating one of those actual game frames.

To regenerate on Linux with a C++20 compiler, Python and Pillow:

```sh
mkdir -p .preview-build/frames
c++ -std=c++20 -O2 src/Chip8Core.cpp scripts/capture_preview.cpp -o .preview-build/capture_preview
.preview-build/capture_preview .preview-build/frames
python scripts/render_preview.py --frames .preview-build/frames
```

Pillow is needed only for regenerating these media files, not for playing, building the application or running core/ROM verification. These original promotional assets use the project's MIT license.
