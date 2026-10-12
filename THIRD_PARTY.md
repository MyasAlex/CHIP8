# Licenses and provenance

The emulator core, Windows frontend, local unit tests, assembler script, and the original `Pong-2Players.ch8` / `Studio-Demo.ch8` are distributed under the project's MIT license. The new core was implemented for this project; C-Octo was used as an external comparison target, not incorporated into the emulator.

`roms/Pong.ch8` and `roms/Breakout.ch8` are the unchanged files from the user's supplied `CHIP8-smooth-rendering.zip`. Their previous notice described them as a public-domain game-pack source without an exact upstream link. That notice is retained in `roms/ORIGINAL_NOTICE.txt`; this release does not assign a new copyright owner or assert a newly verified license for those two files.

In the full source archive, `third_party/timendus` is an unmodified snapshot of [Timendus/chip8-test-suite](https://github.com/Timendus/chip8-test-suite), commit `742e9eac9f5d44466c1c9d83fcaa8158c6683cb1`. Its own GPL-3.0 license, sources, metadata and notices are included in that directory. The Corax+ source also contains its specific MIT notice. These programs are separate guest ROMs used to test the interpreter; they are not linked into the emulator executable or included in the small portable Windows package.

In the full source archive, `tests/golden` contains framebuffer captures from the corresponding guest test programs, produced with [C-Octo](https://github.com/JohnEarnest/c-octo) as the external reference. Keep the Timendus notices when redistributing these test assets. The reference core itself is not bundled here.

Portable Windows releases statically link the compiler runtime. The GitHub Actions build uses MSVC; alternative MinGW-w64/GCC builds use the GCC Runtime Library Exception. The application loads Windows system components and optionally the system XInput DLL. No third-party UI framework is included.

Primary behavior references: [XO-CHIP specification](https://github.com/JohnEarnest/Octo/blob/gh-pages/docs/XO-ChipSpecification.md), [Timendus test descriptions](https://github.com/Timendus/chip8-test-suite), [CHIP-8 database quirk descriptions](https://chip-8.github.io/database/).

## Offline game catalog in 3.0

`roms/catalog/community` contains 104 compiled ROMs and the original metadata from [JohnEarnest/chip8Archive](https://github.com/JohnEarnest/chip8Archive), commit `761e3ffc63f43e6a849e80714122feff2afca208`. The archive's upstream README declares the whole repository CC0-1.0; it is included as `UPSTREAM-README.md`. `roms/catalog/covers` contains unchanged original cover images from that snapshot. Author attribution remains in the metadata and UI.

`roms/catalog/classics` preserves the **entire, unchanged** [kripod/chip8-roms](https://github.com/kripod/chip8-roms) snapshot, commit `c723a9ed1205a215c5b1e45e994eb54acc243c9e`, including its README, CHANGELOG and all individual notices. Its README permits redistribution of the complete package in its original form. Some programs retain their authors' specific conditions; this collection is **not** relabeled CC0 or MIT. Keep the complete directory and notices together when redistributing it.

`third_party/chip8-database` contains the original JSON, README and LICENSE.md from [chip-8/chip-8-database](https://github.com/chip-8/chip-8-database), commit `4b2a2a33f3392d813e8be4c7b955838d0797b425`. Code and JSON use its MIT license; descriptions retain the copyrights of their credited authors. The generated compatibility table does not expand the emulator's implemented hardware platforms.

`roms/catalog/SOURCES.json` records the source commits and downloaded archive SHA-256 values. `catalog.json` records each compiled ROM's SHA-1/SHA-256, origin, author, profile and distribution notice. The catalog deduplicates programs for browsing while retaining every file of the original classic collection. It is not an exhaustive historical archive.

The GIF writer, settings container, input replay and rewind codec are original MIT project code. MP4 uses dynamically loaded Windows Media Foundation components; no external encoder is bundled. Pillow and imageio-ffmpeg are used only by verification scripts and are not linked into or packaged with the application.
