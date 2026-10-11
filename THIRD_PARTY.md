# Licenses and provenance

The emulator core, Windows frontend, local unit tests, assembler script, and the original `Pong-2Players.ch8` / `Studio-Demo.ch8` are distributed under the project's MIT license. The new core was implemented for this project; C-Octo was used as an external comparison target, not incorporated into the emulator.

`roms/Pong.ch8` and `roms/Breakout.ch8` are the unchanged files from the user's supplied `CHIP8-smooth-rendering.zip`. Their previous notice described them as a public-domain game-pack source without an exact upstream link. That notice is retained in `roms/ORIGINAL_NOTICE.txt`; this release does not assign a new copyright owner or assert a newly verified license for those two files.

In the full source archive, `third_party/timendus` is an unmodified snapshot of [Timendus/chip8-test-suite](https://github.com/Timendus/chip8-test-suite), commit `742e9eac9f5d44466c1c9d83fcaa8158c6683cb1`. Its own GPL-3.0 license, sources, metadata and notices are included in that directory. The Corax+ source also contains its specific MIT notice. These programs are separate guest ROMs used to test the interpreter; they are not linked into the emulator executable or included in the small portable Windows package.

In the full source archive, `tests/golden` contains framebuffer captures from the corresponding guest test programs, produced with [C-Octo](https://github.com/JohnEarnest/c-octo) as the external reference. Keep the Timendus notices when redistributing these test assets. The reference core itself is not bundled here.

Portable Windows releases statically link the compiler runtime. The GitHub Actions build uses MSVC; alternative MinGW-w64/GCC builds use the GCC Runtime Library Exception. The application loads Windows system components and optionally the system XInput DLL. No third-party UI framework is included.

Primary behavior references: [XO-CHIP specification](https://github.com/JohnEarnest/Octo/blob/gh-pages/docs/XO-ChipSpecification.md), [Timendus test descriptions](https://github.com/Timendus/chip8-test-suite), [CHIP-8 database quirk descriptions](https://chip-8.github.io/database/).
