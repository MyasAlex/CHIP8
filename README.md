# Эмулятор CHIP-8 для Visual Studio

Готовый учебный CHIP-8-эмулятор под Windows/Visual Studio.

## Запуск

1. Распакуйте ZIP.
2. Дважды нажмите `CHIP8.sln`.
3. В Visual Studio нажмите зелёную кнопку `▶` (проект CHIP8.Emulator должен быть запускаемым).

CMake и SDL2 не требуются.

## Управление

CHIP-8 клавиатура:

```
1 2 3 4
Q W E R
A S D F
Z X C V
```

В эмуляторе это соответствует:

```
1 2 3 C
4 5 6 D
7 8 9 E
A 0 B F
```

Дополнительно:

- `P` — пауза
- `F1` — сброс
- `Esc` — выход
- `Ctrl+O` — открыть ROM

## Игры

В папке `roms` находятся `Pong.ch8` и `Breakout.ch8`.
Их можно открыть через меню **Игры** или через **Файл → Открыть ROM**.

## Архитектура

- `src/Chip8Core.*` — CPU, память, стек, таймеры, клавиатура и дисплей.
- `src/WindowsMain.cpp` — окно Win32, ввод, вывод и звук.
- `tests/Chip8Tests.cpp` — автоматические тесты ядра.


## Timing
The emulator uses a default CPU rate of 700 instructions/second and CHIP-8 timers at 60 Hz. This is intentionally conservative for classic ROMs such as Pong and Breakout.

## Timing / display

The emulator uses a simple CHIP-8 draw-pending model: a draw instruction marks the display dirty, and the Windows front-end presents the newest complete framebuffer immediately. The CPU target remains 700 instructions/s and the CHIP-8 timers are updated independently at 60 Hz.

The project intentionally does not use a display-generation "quiet period" heuristic, because that can suppress normal continuous-motion ROMs such as Pong.
