#include "Chip8Core.hpp"
#include "CatalogData.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=3)return 2;const std::filesystem::path root(argv[1]);std::ofstream report(argv[2]);
    if(!report)return 3;report<<"[\n";unsigned tested=0,failed=0,skipped=0;
    for(const auto& program:chip8_catalog::Games) {
        if(!program.runnable){++skipped;continue;}
        std::ifstream file(root/std::filesystem::path(reinterpret_cast<const char8_t*>(program.path)),std::ios::binary);
        const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
        chip8::Chip8 cpu(chip8::Config::fromQuirks(static_cast<chip8::Profile>(program.profile),static_cast<std::uint8_t>(program.quirks)));
        std::string error;if(!file||!cpu.loadRom(bytes))error="Missing or rejected ROM";
        chip8::Scheduler clock(program.hz);cpu.seedRandom(12345);
        for(int frame=0;error.empty()&&frame<300&&!cpu.halted();++frame)clock.advance(cpu,1.0/60);
        if(error.empty())error=cpu.error();if(!error.empty()){++failed;std::cerr<<program.title<<": "<<error<<"\n";}
        if(tested++)report<<",\n";
        report<<" {\"sha1\":"<<std::quoted(program.sha1)<<",\"title\":"<<std::quoted(program.title)<<",\"error\":"<<std::quoted(error)<<",\"instructions\":"<<cpu.instructionCount()<<",\"halted\":"<<(cpu.halted()?"true":"false")<<"}";
    }
    report<<"\n]\n";std::cout<<"Catalog bounded boot: "<<tested<<" programs, "<<failed<<" interpreter errors; "<<skipped<<" other platforms. Not a complete playthrough.\n";return failed?1:0;
}
