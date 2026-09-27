#include "Chip8Core.hpp"
#include "DemoRom.hpp"
#include "resource.h"

#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <mmsystem.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Winmm.lib")

namespace {

constexpr int IDM_FILE_OPEN = 1001;
constexpr int IDM_FILE_DEMO = 1002;
constexpr int IDM_FILE_RESET = 1003;
constexpr int IDM_FILE_OPEN_FOLDER = 1004;
constexpr int IDM_FILE_EXIT = 1005;
constexpr int IDM_GAME_PONG = 1101;
constexpr int IDM_GAME_BREAKOUT = 1102;
constexpr int IDM_GAME_ROM_FOLDER = 1103;
constexpr int IDM_HELP_ABOUT = 1201;

constexpr wchar_t WindowClassName[] = L"Chip8SmoothEmulatorWindow";

constexpr int Scale = 16;
constexpr int ScreenWidth = static_cast<int>(chip8::Chip8::DisplayWidth) * Scale;
constexpr int ScreenHeight = static_cast<int>(chip8::Chip8::DisplayHeight) * Scale;

constexpr double CpuHz = 700.0;
constexpr double TimerHz = 60.0;
constexpr double RenderHz = 120.0;

chip8::Chip8 g_chip8;
HWND g_hwnd = nullptr;
bool g_paused = false;
std::wstring g_rom_name = L"Встроенная демонстрация";
std::filesystem::path g_rom_directory;

class BackBuffer {
public:
    ~BackBuffer() {
        destroy();
    }

    BackBuffer(const BackBuffer&) = delete;
    BackBuffer& operator=(const BackBuffer&) = delete;

    bool ensure(HDC referenceDc, int width, int height) {
        if (width <= 0 || height <= 0) {
            destroy();
            return false;
        }

        if (dc_ && bitmap_ && bits_ && width_ == width && height_ == height) {
            return true;
        }

        destroy();

        dc_ = CreateCompatibleDC(referenceDc);
        if (!dc_) return false;

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height; // top-down DIB
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        bitmap_ = CreateDIBSection(
            referenceDc,
            &bmi,
            DIB_RGB_COLORS,
            &bits_,
            nullptr,
            0);

        if (!bitmap_ || !bits_) {
            destroy();
            return false;
        }

        oldBitmap_ = SelectObject(dc_, bitmap_);
        if (!oldBitmap_) {
            destroy();
            return false;
        }

        width_ = width;
        height_ = height;
        stride_ = width_;
        return true;
    }

    void destroy() {
        if (dc_) {
            if (oldBitmap_) {
                SelectObject(dc_, oldBitmap_);
                oldBitmap_ = nullptr;
            }
            DeleteDC(dc_);
            dc_ = nullptr;
        }

        if (bitmap_) {
            DeleteObject(bitmap_);
            bitmap_ = nullptr;
        }

        bits_ = nullptr;
        width_ = 0;
        height_ = 0;
        stride_ = 0;
    }

    HDC dc() const { return dc_; }
    std::uint32_t* pixels() const {
        return static_cast<std::uint32_t*>(bits_);
    }
    int width() const { return width_; }
    int height() const { return height_; }
    int stride() const { return stride_; }

private:
    HDC dc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ oldBitmap_ = nullptr;
    void* bits_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    int stride_ = 0;
};

BackBuffer g_back_buffer;
std::uint64_t g_rendered_generation = std::numeric_limits<std::uint64_t>::max();

class ToneThread {
public:
    ToneThread() : worker_([this] { run(); }) {}

    ~ToneThread() {
        stop_.store(true, std::memory_order_relaxed);
        worker_.join();
    }

    void setEnabled(bool enabled) {
        enabled_.store(enabled, std::memory_order_relaxed);
    }

private:
    void run() {
        while (!stop_.load(std::memory_order_relaxed)) {
            if (enabled_.load(std::memory_order_relaxed)) {
                Beep(440, 10);
            } else {
                Sleep(10);
            }
        }
    }

    std::atomic<bool> enabled_{false};
    std::atomic<bool> stop_{false};
    std::thread worker_;
};

ToneThread g_tone;

std::filesystem::path executableDirectory() {
    wchar_t path[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return L".";
    return std::filesystem::path(path).parent_path();
}

std::filesystem::path findRomDirectory() {
    const auto exeDir = executableDirectory();
    std::filesystem::path current = exeDir;

    for (int level = 0; level < 6; ++level) {
        const auto candidate = current / L"roms";
        if (std::filesystem::exists(candidate)) return candidate;
        if (current == current.root_path()) break;
        current = current.parent_path();
    }

    return exeDir / L"roms";
}

void updateTitle() {
    std::wstring title = L"Эмулятор CHIP-8 — " + g_rom_name;
    if (g_paused) title += L" [ПАУЗА]";
    if (g_chip8.halted()) title += L" [ОШИБКА]";
    SetWindowTextW(g_hwnd, title.c_str());
}

void showError(const std::wstring& message) {
    MessageBoxW(g_hwnd, message.c_str(), L"Эмулятор CHIP-8", MB_OK | MB_ICONERROR);
}

bool loadBytes(const std::filesystem::path& path, std::vector<std::uint8_t>& bytes) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;

    const std::streamsize size = file.tellg();
    if (size < 0) return false;

    file.seekg(0, std::ios::beg);
    bytes.resize(static_cast<std::size_t>(size));

    if (size > 0 && !file.read(reinterpret_cast<char*>(bytes.data()), size)) {
        return false;
    }

    return true;
}

void invalidateFrame() {
    if (g_hwnd) {
        InvalidateRect(g_hwnd, nullptr, FALSE);
    }
}

bool buildBackBufferFrame() {
    if (!g_hwnd) return false;

    HDC windowDc = GetDC(g_hwnd);
    if (!windowDc) return false;

    RECT client{};
    GetClientRect(g_hwnd, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;

    const bool ready = g_back_buffer.ensure(windowDc, width, height);
    if (!ready) {
        ReleaseDC(g_hwnd, windowDc);
        return false;
    }

    auto* pixels = g_back_buffer.pixels();
    const std::uint32_t background = RGB(18, 18, 18);
    const std::uint32_t foreground = RGB(235, 245, 255);

    std::fill_n(
        pixels,
        static_cast<std::size_t>(g_back_buffer.width()) *
            static_cast<std::size_t>(g_back_buffer.height()),
        background);

    const auto& screen = g_chip8.display();
    const int logicalWidth = static_cast<int>(chip8::Chip8::DisplayWidth);
    const int logicalHeight = static_cast<int>(chip8::Chip8::DisplayHeight);

    if (width == ScreenWidth && height == ScreenHeight) {
        for (int y = 0; y < logicalHeight; ++y) {
            for (int x = 0; x < logicalWidth; ++x) {
                if (!screen[static_cast<std::size_t>(y) * logicalWidth + x]) {
                    continue;
                }

                for (int sy = 0; sy < Scale; ++sy) {
                    auto* row =
                        pixels +
                        static_cast<std::size_t>(y * Scale + sy) *
                            static_cast<std::size_t>(g_back_buffer.stride()) +
                        static_cast<std::size_t>(x * Scale);

                    std::fill_n(row, Scale, foreground);
                }
            }
        }
    } else {
        const int pixelWidth = std::max(1, width / logicalWidth);
        const int pixelHeight = std::max(1, height / logicalHeight);

        for (int y = 0; y < logicalHeight; ++y) {
            for (int x = 0; x < logicalWidth; ++x) {
                if (!screen[static_cast<std::size_t>(y) * logicalWidth + x]) {
                    continue;
                }

                for (int sy = 0; sy < pixelHeight; ++sy) {
                    const int py = y * pixelHeight + sy;
                    if (py >= height) continue;

                    auto* row =
                        pixels +
                        static_cast<std::size_t>(py) *
                            static_cast<std::size_t>(g_back_buffer.stride());

                    const int px = x * pixelWidth;
                    const int count = std::min(pixelWidth, width - px);
                    if (px >= 0 && count > 0) {
                        std::fill_n(row + px, count, foreground);
                    }
                }
            }
        }
    }

    ReleaseDC(g_hwnd, windowDc);
    return true;
}

void publishLatestFrame() {
    const auto generation = g_chip8.displayGeneration();
    if (generation == g_rendered_generation) return;

    if (buildBackBufferFrame()) {
        g_rendered_generation = generation;
        invalidateFrame();
    }
}

bool loadRomPath(const std::filesystem::path& path) {
    std::vector<std::uint8_t> rom;

    if (!loadBytes(path, rom)) {
        showError(L"Не удалось прочитать ROM:\n" + path.wstring());
        return false;
    }

    if (!g_chip8.loadRom(rom)) {
        const std::string err = g_chip8.error();
        showError(std::wstring(err.begin(), err.end()));
        return false;
    }

    g_paused = false;
    g_rom_name = path.filename().wstring();
    updateTitle();

    g_rendered_generation = std::numeric_limits<std::uint64_t>::max();
    publishLatestFrame();
    return true;
}

bool loadRomFile() {
    wchar_t fileName[MAX_PATH]{};

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFilter =
        L"CHIP-8 ROM (*.ch8;*.rom)\0*.ch8;*.rom\0"
        L"Все файлы (*.*)\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = g_rom_directory.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Открыть CHIP-8 ROM";

    return GetOpenFileNameW(&ofn) ? loadRomPath(fileName) : false;
}

void loadDemo() {
    const auto rom = chip8_demo::makeRom();

    if (!g_chip8.loadRom(rom)) {
        const std::string err = g_chip8.error();
        showError(std::wstring(err.begin(), err.end()));
        return;
    }

    g_paused = false;
    g_rom_name = L"Встроенная демонстрация";
    updateTitle();

    g_rendered_generation = std::numeric_limits<std::uint64_t>::max();
    publishLatestFrame();
}

void resetEmulator() {
    g_chip8.reset();
    g_paused = false;
    g_rom_name = L"Сброшено (ROM не загружен)";
    updateTitle();

    g_rendered_generation = std::numeric_limits<std::uint64_t>::max();
    publishLatestFrame();
}

void openRomFolder() {
    std::error_code ec;
    std::filesystem::create_directories(g_rom_directory, ec);

    ShellExecuteW(
        g_hwnd,
        L"open",
        g_rom_directory.c_str(),
        nullptr,
        nullptr,
        SW_SHOWNORMAL);
}

std::uint8_t mapKey(WPARAM key) {
    switch (key) {
    case '1': return 0x1;
    case '2': return 0x2;
    case '3': return 0x3;
    case '4': return 0xC;
    case 'Q': return 0x4;
    case 'W': return 0x5;
    case 'E': return 0x6;
    case 'R': return 0xD;
    case 'A': return 0x7;
    case 'S': return 0x8;
    case 'D': return 0x9;
    case 'F': return 0xE;
    case 'Z': return 0xA;
    case 'X': return 0x0;
    case 'C': return 0xB;
    case 'V': return 0xF;
    default: return 0xFF;
    }
}

void paint(HWND hwnd, HDC hdc) {
    RECT client{};
    GetClientRect(hwnd, &client);

    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) return;

    if (!g_back_buffer.ensure(hdc, width, height)) {
        FillRect(
            hdc,
            &client,
            static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        return;
    }

    BitBlt(
        hdc,
        0,
        0,
        width,
        height,
        g_back_buffer.dc(),
        0,
        0,
        SRCCOPY);
}

HMENU makeMenu() {
    HMENU bar = CreateMenu();
    HMENU file = CreatePopupMenu();
    HMENU games = CreatePopupMenu();
    HMENU help = CreatePopupMenu();

    AppendMenuW(file, MF_STRING, IDM_FILE_OPEN, L"Открыть ROM...\tCtrl+O");
    AppendMenuW(file, MF_STRING, IDM_FILE_DEMO, L"Встроенная демонстрация");
    AppendMenuW(file, MF_STRING, IDM_FILE_RESET, L"Сбросить\tF1");
    AppendMenuW(file, MF_STRING, IDM_FILE_OPEN_FOLDER, L"Открыть папку ROM");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, IDM_FILE_EXIT, L"Выход");

    AppendMenuW(games, MF_STRING, IDM_GAME_PONG, L"Pong");
    AppendMenuW(games, MF_STRING, IDM_GAME_BREAKOUT, L"Breakout");
    AppendMenuW(games, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(games, MF_STRING, IDM_GAME_ROM_FOLDER, L"Все ROM в папке...");

    AppendMenuW(help, MF_STRING, IDM_HELP_ABOUT, L"О программе");

    AppendMenuW(
        bar,
        MF_POPUP,
        reinterpret_cast<UINT_PTR>(file),
        L"Файл");
    AppendMenuW(
        bar,
        MF_POPUP,
        reinterpret_cast<UINT_PTR>(games),
        L"Игры");
    AppendMenuW(
        bar,
        MF_POPUP,
        reinterpret_cast<UINT_PTR>(help),
        L"Справка");

    return bar;
}

void clearKeys() {
    for (std::uint8_t key = 0; key < chip8::Chip8::KeyCount; ++key) {
        g_chip8.setKey(key, false);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        g_hwnd = hwnd;
        SetMenu(hwnd, makeMenu());
        updateTitle();
        return 0;

    case WM_SIZE:
        g_rendered_generation = std::numeric_limits<std::uint64_t>::max();
        publishLatestFrame();
        return 0;

    case WM_KEYDOWN: {
        if (wParam == VK_F1) {
            resetEmulator();
            return 0;
        }

        if (wParam == 'P') {
            g_paused = !g_paused;
            updateTitle();
            return 0;
        }

        if (wParam == VK_ESCAPE) {
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
            return 0;
        }

        if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'O') {
            loadRomFile();
            return 0;
        }

        if ((lParam & 0x40000000) != 0) {
            return 0;
        }

        const auto key = mapKey(wParam);
        if (key != 0xFF) {
            g_chip8.setKey(key, true);
        }
        return 0;
    }

    case WM_KEYUP: {
        const auto key = mapKey(wParam);
        if (key != 0xFF) {
            g_chip8.setKey(key, false);
        }
        return 0;
    }

    case WM_KILLFOCUS:
        clearKeys();
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        paint(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_FILE_OPEN:
            loadRomFile();
            return 0;
        case IDM_FILE_DEMO:
            loadDemo();
            return 0;
        case IDM_FILE_RESET:
            resetEmulator();
            return 0;
        case IDM_FILE_OPEN_FOLDER:
            openRomFolder();
            return 0;
        case IDM_FILE_EXIT:
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
            return 0;
        case IDM_GAME_PONG:
            loadRomPath(g_rom_directory / L"Pong.ch8");
            return 0;
        case IDM_GAME_BREAKOUT:
            loadRomPath(g_rom_directory / L"Breakout.ch8");
            return 0;
        case IDM_GAME_ROM_FOLDER:
            openRomFolder();
            return 0;
        case IDM_HELP_ABOUT:
            MessageBoxW(
                hwnd,
                L"Эмулятор CHIP-8\n\n"
                L"Учебный проект на C++20 и Win32.\n"
                L"Экран: 64×32.\n"
                L"Клавиши: 1-4 / QWER / ASDF / ZXCV.\n"
                L"P — пауза, F1 — сброс, Esc — выход.\n\n"
                L"Рендер: постоянный 32-битный back buffer.\n"
                L"Новый кадр собирается полностью до одного BitBlt,\n"
                L"а отображение ограничено стабильными 120 Гц.",
                L"О программе",
                MB_OK | MB_ICONINFORMATION);
            return 0;
        default:
            break;
        }
        break;

    case WM_DESTROY:
        clearKeys();
        g_tone.setEnabled(false);
        g_back_buffer.destroy();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool createMainWindow(HINSTANCE instance, int show) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = WindowClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hbrBackground = nullptr;

    if (!RegisterClassW(&wc)) {
        return false;
    }

    RECT rect{0, 0, ScreenWidth, ScreenHeight};

    AdjustWindowRectEx(
        &rect,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        TRUE,
        0);

    g_hwnd = CreateWindowExW(
        0,
        WindowClassName,
        L"Эмулятор CHIP-8",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!g_hwnd) {
        return false;
    }

    ShowWindow(g_hwnd, show);
    UpdateWindow(g_hwnd);
    return true;
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    SetProcessDPIAware();
    timeBeginPeriod(1);

    g_rom_directory = findRomDirectory();

    if (!createMainWindow(instance, show)) {
        MessageBoxW(
            nullptr,
            L"Не удалось создать окно CHIP-8.",
            L"Ошибка",
            MB_OK | MB_ICONERROR);

        timeEndPeriod(1);
        return 1;
    }

    loadDemo();

    using clock = std::chrono::steady_clock;

    auto previous = clock::now();
    double cpuAccumulator = 0.0;
    double timerAccumulator = 0.0;
    double renderAccumulator = 0.0;

    constexpr double cpuStep = 1.0 / CpuHz;
    constexpr double timerStep = 1.0 / TimerHz;
    constexpr double renderStep = 1.0 / RenderHz;

    bool running = true;

    while (running) {
        MSG message{};

        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                running = false;
                break;
            }

            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        if (!running) break;

        const auto now = clock::now();

        double dt =
            std::chrono::duration<double>(now - previous).count();

        previous = now;

        // Prevent a debugger break, drag operation, sleep, or similar host
        // stall from producing a huge emulation catch-up burst.
        dt = std::clamp(dt, 0.0, 0.050);

        if (!g_paused && !g_chip8.halted()) {
            cpuAccumulator += dt;
            timerAccumulator += dt;
        }

        renderAccumulator += dt;

        // Fixed-rate CPU execution. The old 8-step cap could permanently
        // undershoot the target after a small host scheduling hitch.
        int cpuSteps = 0;

        while (!g_paused &&
               !g_chip8.halted() &&
               cpuAccumulator >= cpuStep &&
               cpuSteps < 64) {
            g_chip8.step();
            cpuAccumulator -= cpuStep;
            ++cpuSteps;
        }

        // CHIP-8 delay/sound timers advance independently at 60 Hz.
        while (!g_paused &&
               !g_chip8.halted() &&
               timerAccumulator >= timerStep) {
            g_chip8.tickTimers();
            timerAccumulator -= timerStep;
        }

        // The CPU is allowed to draw whenever the ROM asks. We do not expose
        // every intermediate framebuffer, however. The newest complete frame
        // is published to the persistent back buffer at a stable 120 Hz.
        if (renderAccumulator >= renderStep) {
            while (renderAccumulator >= renderStep) {
                renderAccumulator -= renderStep;
            }

            publishLatestFrame();
        } else if (g_chip8.displayGeneration() != g_rendered_generation) {
            publishLatestFrame();
        }

        g_tone.setEnabled(
            g_chip8.soundTimer() > 0 &&
            !g_chip8.halted() &&
            !g_paused);

        Sleep(1);
    }

    timeEndPeriod(1);
    return 0;
}
