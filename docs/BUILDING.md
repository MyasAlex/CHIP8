# Build and verify

The ready-to-run application is in [GitHub Releases](https://github.com/MyasAlex/CHIP8/releases/latest). These instructions are for contributors.

## CMake

Use CMake 3.20 or newer and a C++20 compiler. On Windows, install Visual Studio's Desktop development with C++ workload and Windows SDK. Python 3 is required for offline ROM comparisons and release packaging.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

On Windows with a multi-configuration generator, executables are in build/Release. On Linux, the portable core, CHIP8.Tests and CHIP8.RomRunner are built; the Win32 frontend is not.

The core check contains 47 groups. The offline ROM check runs 31 Timendus scenarios, checks success glyphs and compares complete framebuffers with committed C-Octo references. The external suite is committed, so verification does not fetch ROMs at runtime.

## Visual Studio solution

Open CHIP8.sln, select **Release / x64**, and choose **CHIP8.Emulator** as the startup project. The project uses the installed Visual Studio toolset. Output is in bin/x64/Release. build-windows.bat provides an alternative command-line build.

## Native Windows check

```powershell
$process = Start-Process .\build\Release\CHIP8.Emulator.exe -ArgumentList '--smoke-test', 'build/smoke' -Wait -PassThru
if ($process.ExitCode -ne 0) { throw 'Windows smoke check failed' }
Get-Content build/smoke/smoke-result.txt
```

This exercises HWND/GDI, input messages, save/load, fullscreen and PNG paths. It does not validate physical sound output, controller hardware or hardware VSync.

## Package and publish

```powershell
python scripts/package_release.py --exe build/Release/CHIP8.Emulator.exe --output dist
```

The script checks the executable's x64 Windows GUI header, verifies the packaged files and writes SHA256SUMS.txt and build metadata. The ZIP's stable name is CHIP8-Studio-Windows-x64.zip so the README's latest-release link continues to work.

version.json is the release manifest. Keep it in sync with CMake, Windows resources and the changelog. Add bilingual notes at docs/releases/<tag>.md.

Pull requests run Linux and Windows verification and produce a Windows package artifact. After verified changes reach master, the same workflow creates a draft release with the ZIP and checksums, then publishes it after upload succeeds. Only that final job has contents: write. An existing tag is kept unchanged. A newer release requires a new version and tag. No personal access token is required.

## Sanitizers

```sh
cmake -S . -B build/sanitized -DCMAKE_BUILD_TYPE=Debug -DCHIP8_SANITIZERS=ON
cmake --build build/sanitized --parallel
ctest --test-dir build/sanitized --output-on-failure
```

## Structure

| Path | Purpose |
| --- | --- |
| src/Chip8Core.hpp, src/Chip8Core.cpp | CPU, profiles, state format, scheduler and presentation. |
| src/WindowsMain.cpp | Win32 window, input, audio and player tools. |
| scripts/generate_demo.py | Original Pong/demo assembler and embedded ROM arrays. |
| tests | Core checks, ROM runner and reference framebuffers. |
| third_party/timendus | Unmodified external test suite and its license. |
| .github/workflows/build-windows.yml | Cross-platform checks and Windows release pipeline. |

Historical preparation records are in [TECHNICAL_RU.md](TECHNICAL_RU.md) and [ANALYSIS_RU.md](ANALYSIS_RU.md). The Actions logs describe each subsequent build.
