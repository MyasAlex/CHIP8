// Actual emulator frames with scripted two-player input for documentation.
// Build: c++ -std=c++20 -O2 src/Chip8Core.cpp scripts/capture_preview.cpp -o capture_preview
#include "../src/Chip8Core.hpp"
#include "../src/DemoRom.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

int main(int argc, char** argv) try {
    if (argc != 2) throw std::runtime_error("Usage: capture_preview OUTPUT_DIRECTORY");
    const std::filesystem::path output(argv[1]);
    std::filesystem::create_directories(output);
    chip8::Chip8 cpu;
    if (!cpu.loadRom(chip8_demo::Pong2)) throw std::runtime_error("Demo ROM failed to load");
    cpu.seedRandom(0xC81u);
    chip8::Scheduler scheduler(6000);
    chip8::Presenter presenter;
    auto frame = [&] { presenter.publish(cpu, true); };
    auto display = [&] { presenter.observe(cpu); };
    for (int n = 0; n < 360; ++n) {
        const auto& v = cpu.registers();
        const int target = std::max(6, std::min(27, static_cast<int>(v[5]) - 2));
        cpu.setKey(1, v[1] > target);
        cpu.setKey(4, v[1] < target);
        cpu.setKey(12, v[3] > target);
        cpu.setKey(13, v[3] < target);
        scheduler.advance(cpu, 1.0/60.0, frame, display);
        if (cpu.halted()) throw std::runtime_error(cpu.error());
        if (n % 2 != 1) continue;
        std::ostringstream name;
        name << "frame-" << std::setw(3) << std::setfill('0') << n/2 << ".ppm";
        std::ofstream image(output / name.str(), std::ios::binary);
        image << "P6\n64 32\n255\n";
        constexpr unsigned char colors[4][3] = {{11,20,32},{216,243,255},{102,179,255},{255,189,134}};
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 64; ++x)
                image.write(reinterpret_cast<const char*>(colors[presenter.display()[y*128+x]&3]), 3);
        if (!image) throw std::runtime_error("Frame write failed");
    }
    std::cout << "Captured 180 actual emulator frames (6 seconds).\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
