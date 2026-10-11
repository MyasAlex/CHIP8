# Ready-to-use announcement drafts

These are drafts for the project's owner to publish from their own accounts. Attach docs/media/gameplay.gif or social-preview.png where the platform supports it. The GIF shows real core output with scripted inputs, not the desktop window or an in-app bot.

## English: developer community

**Title:** CHIP-8 Studio: a portable Windows emulator, and what I learned fixing XOR sprite flicker

**Post:**

I have been working on CHIP-8 Studio, a C++20 emulator with a native Win32 frontend. I have prepared a portable Windows release with built-in two-player Pong, save states, rewind and selectable CHIP-8 / Super-CHIP Modern / XO-CHIP profiles.

The interesting rendering problem was classic Pong and Breakout: a moving sprite could vanish for a frame because the ROM erased it and redrew it across a 60 Hz display boundary. Version 2.1 tracks those updates per sprite in a separate presentation layer. It holds an erased sprite until a matching redraw, replaces the old position on redraw, and never changes the guest framebuffer or collision flags. An unmatched hold expires within four frame boundaries; exact display remains available.

The repository includes 47 core test groups and 31 offline Timendus scenarios with C-Octo reference framebuffer comparisons. CI also exercises native Windows window/input/save-state/fullscreen/PNG paths.

I'd appreciate feedback from emulator developers on the rendering tradeoff and reports of ROMs that behave differently under the compatibility profiles.

Source and download: https://github.com/MyasAlex/CHIP8
Rendering details: https://github.com/MyasAlex/CHIP8/blob/master/docs/FLICKER.md

The application interface is currently Russian; player documentation is available in English and Russian.

## English: short announcement

CHIP-8 Studio 2.1 is ready: a portable Windows emulator with built-in two-player Pong, save states, rewind and CHIP-8 / Super-CHIP / XO-CHIP profiles. This release improves XOR sprite stabilization in classic Pong and Breakout.

Download, extract and open the EXE. Free and open source:
https://github.com/MyasAlex/CHIP8/releases/latest

The interface is Russian; English and Russian player guides are included.

## По-русски: полный анонс

**Заголовок:** CHIP-8 Studio 2.1 — эмулятор для Windows: скачал, распаковал, играешь

**Текст:**

Подготовил CHIP-8 Studio 2.1 — бесплатный эмулятор CHIP-8 для Windows 10/11 x64.

Встроенный Pong на двоих запускается сразу: W/S — левая ракетка, стрелки — правая. Есть классический Pong, Breakout и загрузка своих ROM через Ctrl+O или перетаскивание.

Возможности:
- профили CHIP-8, Super-CHIP Modern и XO-CHIP;
- сохранения F5/F8 и перемотка Backspace;
- полный экран, PNG, отладка и поддержка XInput;
- стабилизация мерцания движущихся спрайтов с отдельным точным режимом.

Visual Studio и установка не нужны: распаковываешь ZIP и запускаешь CHIP8.Emulator.exe.

Исходники и скачать:
https://github.com/MyasAlex/CHIP8

Буду рад отзывам: напишите, на какой игре проверяли, какой профиль и скорость использовали. Если нашли ошибку, на GitHub есть удобная форма отчёта.

## По-русски: короткий пост

Выпустил CHIP-8 Studio 2.1 для Windows. Встроенный Pong на двоих, сохранения, перемотка, профили CHIP-8 / Super-CHIP / XO-CHIP и исправление пропаданий спрайтов в классическом Pong и Breakout.

Скачать ZIP, распаковать и открыть EXE:
https://github.com/MyasAlex/CHIP8/releases/latest

Бесплатно, исходники открыты. Буду рад проверке на ваших ROM и сообщениям об ошибках.

## Technical article draft

**Title:** Why a CHIP-8 sprite can disappear between frames

CHIP-8's XOR drawing makes moving a sprite slightly different from moving a rectangle in a modern graphics API. The ROM typically draws the sprite at the old coordinates to erase it, updates its position, then draws it again. That sequence is correct inside the guest.

The host display has its own schedule. If it samples the framebuffer after the erase and before the redraw, the object is absent for that frame. Increasing the refresh rate or repainting on every instruction does not necessarily solve the timing mismatch.

In CHIP-8 Studio 2.1, I keep the guest framebuffer authoritative for instruction execution and collision detection. A separate presenter observes each display instruction. When an instruction purely erases a sprite, the presenter remembers those pixels while waiting for a matching redraw. The matching redraw replaces the held position. This prevents old positions from accumulating into a trail.

The matching key includes the draw opcode, sprite bytes and display planes. When candidates otherwise match, the nearest old position wins. Clear, scroll and resolution changes reset pending data. An unmatched erasure expires within four frame boundaries, at most 66.7 ms.

There is an unavoidable tradeoff: a presentation heuristic can conceal a deliberately short blink or slightly delay an object's disappearance. For that reason, exact display is a separate selectable mode, and the test tools always inspect the guest framebuffer.

The regression suite covers independently moving sprites, nearest-candidate matching, clear/scroll/mode changes, XO-CHIP planes and exact publication. Replays of classic Pong and Breakout also compare the guest state with and without presentation stabilization across 1200 frames. The logic remains identical while the display changes.

This is not a universal fix for every ROM. CPU speed, compatibility quirks and the program's own update cadence still matter. My aim is to make the tradeoff visible and testable so a player can choose a steady display while a developer can inspect the exact guest output.

Code and implementation notes:
https://github.com/MyasAlex/CHIP8
https://github.com/MyasAlex/CHIP8/blob/master/docs/FLICKER.md

## Fifteen-second video outline

- 0–3 s: actual app launch and two-player Pong; caption “Download. Extract. Play.”
- 3–7 s: paddle movement; caption “W/S + ↑/↓”.
- 7–11 s: demonstrate save/load and rewind in the real application.
- 11–15 s: fullscreen or PNG capture, then the GitHub download link.

Record the real desktop app for save/load and rewind demonstrations; the prepared core GIF only shows gameplay.
