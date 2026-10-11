#include "../src/Chip8Core.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

// ROM PROFILE CHOICE FRAMES HZ OUTPUT_PREFIX [KEYPAD]
// PROFILE: 0=VIP, 1=Modern, 2=Super-CHIP, 3=XO-CHIP.
int main(int argc,char** argv)try {
    if(argc<8) {std::cerr<<"ROM PROFILE CHOICE FRAMES HZ OUTPUT_PREFIX KEYPAD\n";return 2;}
    std::ifstream file(argv[1],std::ios::binary);
    if(!file)throw std::runtime_error("ROM not found");
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
    chip8::Chip8 cpu(chip8::Config::preset(static_cast<chip8::Profile>(std::stoi(argv[2]))));
    if(!cpu.loadRom(bytes))throw std::runtime_error("ROM rejected");
    cpu.poke(0x1FF,static_cast<std::uint8_t>(std::stoi(argv[3])));
    chip8::Scheduler scheduler(std::stod(argv[5]));
    const int frames=std::stoi(argv[4]),keypad=std::stoi(argv[7]);
    int soundFrames=0,waitFrames=0;
    for(int n=0;n<frames;++n) {
        if(keypad&&n==60)cpu.setKey(5,true);
        if(keypad&&n==66)cpu.setKey(5,false);
        scheduler.advance(cpu,1.0/60.0);
        if(cpu.soundTimer())++soundFrames;
        if(cpu.waitingForKey())++waitFrames;
        if(cpu.halted())throw std::runtime_error(cpu.error());
    }
    const std::filesystem::path prefix(argv[6]);
    std::ofstream memory(prefix.string()+".ram",std::ios::binary);
    memory.write(reinterpret_cast<const char*>(cpu.memory().data()),cpu.memory().size());
    std::ofstream image(prefix.string()+".ppm",std::ios::binary);
    image<<"P6\n"<<cpu.width()<<" "<<cpu.height()<<"\n255\n";
    constexpr unsigned char colors[4][3]={{11,20,32},{216,243,255},{102,179,255},{255,189,134}};
    for(int y=0;y<cpu.height();++y)for(int x=0;x<cpu.width();++x)image.write(reinterpret_cast<const char*>(colors[cpu.display()[y*128+x]&3]),3);
    std::ofstream result(prefix.string()+".json");
    result<<"{\"pc\":"<<cpu.pc()<<",\"instructions\":"<<cpu.instructionCount()<<",\"sound_frames\":"<<soundFrames<<",\"key_wait_frames\":"<<waitFrames<<",\"width\":"<<cpu.width()<<",\"height\":"<<cpu.height()<<"}\n";
    std::cout<<prefix.filename().string()<<" OK ("<<cpu.instructionCount()<<" instructions)\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<"\n";return 1;}
