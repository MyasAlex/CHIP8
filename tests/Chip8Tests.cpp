#include "../src/Chip8Core.hpp"
#include "../src/DemoRom.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

using chip8::Chip8;
using chip8::Config;
using chip8::Profile;
#define CHECK(expr) do { if(!(expr))throw std::runtime_error(std::string(#expr)+" at line "+std::to_string(__LINE__)); } while(false)
namespace {
std::vector<std::uint8_t> rom(std::initializer_list<std::uint16_t> ops) {
    std::vector<std::uint8_t> bytes;
    for(auto op:ops) { bytes.push_back(static_cast<std::uint8_t>(op>>8));bytes.push_back(static_cast<std::uint8_t>(op)); }
    return bytes;
}
void run(Chip8& c,int n) { while(n-->0)CHECK(c.step()); }
void load(Chip8& c,std::initializer_list<std::uint16_t> ops) {CHECK(c.loadRom(rom(ops)));}
bool pixel(const Chip8& c,int x,int y,int plane=1) {return (c.display()[static_cast<std::size_t>(y)*128+x]&plane)!=0;}
void resetLoad() {
    Chip8 c;load(c,{0x602A,0x1200});run(c,1);CHECK(c.registers()[0]==42);const auto generation=c.displayGeneration();c.reset();
    CHECK(c.pc()==512&&c.registers()[0]==0&&c.memory()[0x50]==0xF0);CHECK(c.displayGeneration()>generation);
}
void atomicRom() {
    Chip8 c;load(c,{0x602A,0x1200});run(c,1);auto before=c.saveState();
    CHECK(!c.loadRom(nullptr,4));CHECK(!c.loadRom(std::vector<std::uint8_t>{}));CHECK(!c.loadRom(std::vector<std::uint8_t>(3585)));
    CHECK(c.saveState()==before);CHECK(!c.halted());
}
void exhaustiveMath() {
    // Every pair of 8-bit operands, plus VX/VY aliasing the flag register.
    std::uint64_t cases=0;
    for(int op:{4,5,7})for(auto regs:{std::pair<int,int>{0,1},{0,15},{15,1},{15,15}}) {
        Chip8 c;load(c,{static_cast<std::uint16_t>(0x6000|(regs.first<<8)),static_cast<std::uint16_t>(0x6000|(regs.second<<8)),
                        static_cast<std::uint16_t>(0x8000|(regs.first<<8)|(regs.second<<4)|op),0x1200});
        for(int a=0;a<256;++a)for(int b=0;b<256;++b) {
            CHECK(c.poke(0x201,static_cast<std::uint8_t>(a)));CHECK(c.poke(0x203,static_cast<std::uint8_t>(b)));
            run(c,2);const auto va=c.registers()[regs.first],vb=c.registers()[regs.second];
            const int result=op==4?va+vb:op==5?va-vb:vb-va;
            const std::uint8_t flag=static_cast<std::uint8_t>(op==4?result>255:op==5?va>=vb:vb>=va);
            run(c,1);CHECK(c.registers()[15]==flag);
            CHECK(c.registers()[regs.first]==(regs.first==15?flag:static_cast<std::uint8_t>(result)));
            run(c,1);++cases;
        }
    }
    std::cout<<"  Exhaustive arithmetic cases: "<<cases<<"\n";
}
void shifts() {
    for(auto p:{Profile::Vip,Profile::Modern,Profile::XoChip})for(int value=0;value<256;++value) {
        Chip8 c(Config::preset(p));load(c,{0x6003,static_cast<std::uint16_t>(0x6100|value),0x8016,0x801E});run(c,3);
        const int operand=p==Profile::Modern?3:value;
        CHECK(c.registers()[0]==operand/2&&c.registers()[15]==(operand&1));run(c,1);
        const int operand2=p==Profile::Modern?operand/2:value;
        CHECK(c.registers()[0]==static_cast<std::uint8_t>(operand2*2)&&c.registers()[15]==(operand2>>7));
    }
    Chip8 c;load(c,{0x6FFF,0x8FF6});run(c,2);CHECK(c.registers()[15]==1);
}
void logic() {
    for(auto p:{Profile::Vip,Profile::Modern}) {
        Chip8 c(Config::preset(p));load(c,{0x60A5,0x6F0F,0x80F1,0x60A5,0x6F0F,0x80F2,0x60A5,0x6F0F,0x80F3});
        run(c,3);CHECK(c.registers()[0]==0xAF);CHECK(c.registers()[15]==(p==Profile::Vip?0:15));
        run(c,3);CHECK(c.registers()[0]==5);run(c,3);CHECK(c.registers()[0]==0xAA);
    }
}
void addNoFlag() {Chip8 c;load(c,{0x6FA5,0x60FF,0x7001});run(c,3);CHECK(c.registers()[0]==0&&c.registers()[15]==0xA5);}
void callStack() {Chip8 c;load(c,{0x2206,0x6007,0x1204,0x600A,0x00EE});run(c,1);CHECK(c.stackDepth()==1&&c.pc()==0x206);run(c,2);CHECK(c.pc()==0x202&&c.stackDepth()==0);run(c,1);CHECK(c.registers()[0]==7);}
void stackBounds() {Chip8 c;load(c,{0x2200});run(c,16);CHECK(!c.step()&&c.stackDepth()==16);load(c,{0x00EE});CHECK(!c.step()&&c.stackDepth()==0);}
void skips() {Chip8 c;load(c,{0x6001,0x3001,0x6002,0x6103,0x4104,0x6100,0x5010,0x6207,0x9010,0x6200,0x1200});run(c,7);CHECK(c.registers()[0]==1&&c.registers()[1]==3&&c.registers()[2]==7);}
void jumpQuirk() {Chip8 c;load(c,{0x6002,0x6304,0xB300});run(c,3);CHECK(c.pc()==0x302);c.setConfig(Config::preset(Profile::SuperChip));load(c,{0x6002,0x6304,0xB300});run(c,3);CHECK(c.pc()==0x304);}
void bounds() {Chip8 c;load(c,{0x1FFF});run(c,1);CHECK(!c.step());load(c,{0xAFFF,0xF133});run(c,1);auto mem=c.memory();CHECK(!c.step()&&c.memory()==mem);load(c,{0xAFFF,0xF155});run(c,1);CHECK(!c.step());load(c,{0xAFFF,0xD012});run(c,1);auto display=c.display();CHECK(!c.step()&&c.display()==display);}
void invalidOps() {for(auto op:{0x8128,0x5121,0x9121,0xE012,0xF099}) {Chip8 c;load(c,{static_cast<std::uint16_t>(op)});CHECK(!c.step()&&c.halted()&&!c.error().empty());}}
void bcd() {for(int n=0;n<256;++n) {Chip8 c;load(c,{static_cast<std::uint16_t>(0x6000|n),0xA300,0xF033});run(c,3);CHECK(c.memory()[0x300]==n/100&&c.memory()[0x301]==n/10%10&&c.memory()[0x302]==n%10);}}
void memoryQuirk() {for(auto p:{Profile::Modern,Profile::Vip,Profile::SuperChip,Profile::XoChip}) {Chip8 c(Config::preset(p));load(c,{0x600A,0x610B,0xA300,0xF155,0x6000,0xA300,0xF165});run(c,4);CHECK(c.index()==(c.config().incrementI?0x302:0x300));run(c,3);CHECK(c.registers()[0]==10&&c.registers()[1]==11);}}
void fonts() {Chip8 c(Config::preset(Profile::SuperChip));load(c,{0x60FA,0xF029,0xF030});run(c,2);CHECK(c.index()==0x82);run(c,1);CHECK(c.index()==0x104&&c.memory()[c.index()]==0xFF);}
void timers() {Chip8 c;load(c,{0x6003,0xF015,0xF018,0x1206});run(c,3);for(int n=2;n>=0;--n) {c.tickTimers();CHECK(c.delayTimer()==n&&c.soundTimer()==n);}c.tickTimers();CHECK(c.delayTimer()==0&&c.soundTimer()==0);}
void keys() {Chip8 c;load(c,{0x6001,0xE09E,0x6101,0xE0A1,0x6202,0x1200});c.setKey(1,true);run(c,4);CHECK(c.registers()[1]==0&&c.registers()[2]==2);c.setKey(16,true);CHECK(!c.isKeyPressed(16));}
void keyWait() {Chip8 c;load(c,{0xF20A,0x6009});run(c,1);CHECK(c.waitingForKey());auto count=c.instructionCount();run(c,20);CHECK(c.instructionCount()==count);c.setKey(11,true);CHECK(c.waitingForKey());c.setKey(11,true);CHECK(c.waitingForKey());c.setKey(11,false);CHECK(!c.waitingForKey()&&c.registers()[2]==11);run(c,1);CHECK(c.registers()[0]==9);}
void heldKey() {Chip8 c;load(c,{0xF00A});c.setKey(5,true);run(c,1);CHECK(c.waitingForKey());c.setKey(5,false);CHECK(!c.waitingForKey()&&c.registers()[0]==5);}
void focusKeys() {Chip8 c;load(c,{0xF00A});run(c,1);c.setKey(4,true);c.releaseKeys();CHECK(c.waitingForKey());c.setKey(3,true);c.setKey(3,false);CHECK(!c.waitingForKey()&&c.registers()[0]==3);}
void drawCollision() {Chip8 c;load(c,{0xA300,0x6000,0x6100,0xD011,0xD011});CHECK(c.poke(0x300,0x80));run(c,4);CHECK(pixel(c,0,0)&&c.registers()[15]==0);run(c,1);CHECK(!pixel(c,0,0)&&c.registers()[15]==1);}
void drawVfCoordinates() {Chip8 c;load(c,{0xA300,0x6F05,0x6002,0xDF01});CHECK(c.poke(0x300,0x80));run(c,4);CHECK(pixel(c,5,2)&&!pixel(c,0,2));}
void clipWrap() {for(bool clip:{false,true}) {Chip8 c;auto cfg=c.config();cfg.clipSprites=clip;c.setConfig(cfg);load(c,{0xA300,0x60FF,0x613F,0xD012});CHECK(c.poke(0x300,0xFF));CHECK(c.poke(0x301,0xFF));run(c,4);CHECK(pixel(c,63,31));CHECK(pixel(c,0,0)==!clip&&pixel(c,0,31)==!clip);}}
void vblank() {Chip8 c(Config::preset(Profile::Vip));load(c,{0xA300,0xD011,0xD011});CHECK(c.poke(0x300,0x80));run(c,2);CHECK(c.pc()==0x202&&c.waitingForDraw()&&!pixel(c,0,0));c.tickTimers();run(c,1);CHECK(pixel(c,0,0));run(c,1);CHECK(pixel(c,0,0)&&c.pc()==0x204);c.tickTimers();run(c,1);CHECK(!pixel(c,0,0));}
void resolutions() {Chip8 c(Config::preset(Profile::SuperChip));load(c,{0x00FF,0xA300,0xD010,0x00FE});CHECK(c.poke(0x300,0x80));CHECK(c.poke(0x31E,0x01));run(c,3);CHECK(c.width()==128&&pixel(c,0,0)&&pixel(c,7,15));run(c,1);CHECK(c.width()==64&&std::all_of(c.display().begin(),c.display().end(),[](auto b){return b==0;}));}
void scroll() {Chip8 c(Config::preset(Profile::SuperChip));load(c,{0xA300,0xD011,0x00FB,0x00C3,0x00FC});CHECK(c.poke(0x300,0x80));run(c,3);CHECK(pixel(c,4,0)&&!pixel(c,0,0));run(c,1);CHECK(pixel(c,4,3)&&!pixel(c,4,0));run(c,1);CHECK(pixel(c,0,3));}
void flags() {Chip8 c(Config::preset(Profile::SuperChip));load(c,{0x600C,0xF075,0x6000,0xF085});run(c,4);CHECK(c.registers()[0]==12);c.reset();CHECK(c.flags()[0]==12);load(c,{0xFF75});CHECK(!c.step());}
void xoLongSkip() {Chip8 c(Config::preset(Profile::XoChip));load(c,{0x6001,0x3001,0xF000,0xFFFF,0xF000,0x4567,0x1200});run(c,2);CHECK(c.pc()==0x208);run(c,1);CHECK(c.index()==0x4567);}
void xoMemory() {Chip8 c(Config::preset(Profile::XoChip));load(c,{0xF000,0xFFFE,0x60A5,0x615A,0xF155});run(c,4);CHECK(c.memory()[0xFFFE]==0xA5&&c.memory()[0xFFFF]==0x5A&&c.index()==0);}
void xoRange() {Chip8 c(Config::preset(Profile::XoChip));load(c,{0xA300,0x6001,0x6102,0x6203,0x5202,0x6000,0x6100,0x6200,0x5203});run(c,5);CHECK(c.index()==0x300&&c.memory()[0x300]==3&&c.memory()[0x302]==1);run(c,4);CHECK(c.registers()[0]==1&&c.registers()[2]==3);}
void xoPlanes() {Chip8 c(Config::preset(Profile::XoChip));load(c,{0xA300,0xF301,0xD011,0xF101,0x00E0,0x00D1});CHECK(c.poke(0x300,0x80));CHECK(c.poke(0x301,0xC0));run(c,3);CHECK(c.display()[0]==3&&c.display()[1]==2);run(c,2);CHECK(c.display()[0]==2);run(c,1);CHECK(c.display()[0]==2);}
void xoAudio() {Chip8 c(Config::preset(Profile::XoChip));load(c,{0xA300,0xF002,0x60F7,0xF03A,0xFF75});CHECK(c.poke(0x300,0xA5));run(c,4);CHECK(c.audioPattern()[0]==0xA5&&c.pitch()==247);CHECK(c.poke(0x300,0));CHECK(c.audioPattern()[0]==0xA5);run(c,1);}
void stateRoundtrip() {Chip8 c;load(c,{0xC0FF,0x7001,0x1200});run(c,17);const auto saved=c.saveState();run(c,100);const auto result=c.saveState();std::string problem;CHECK(c.loadState(saved,problem));run(c,100);CHECK(c.saveState()==result);}
void stateValidation() {Chip8 c;load(c,{0x600A,0x1200});run(c,1);auto saved=c.saveState();std::string problem;auto corrupt=saved;corrupt[70]^=1;CHECK(!c.loadState(corrupt,problem)&&c.saveState()==saved);corrupt.resize(60);CHECK(!c.loadState(corrupt,problem));Chip8 other;load(other,{0x600B,0x1200});CHECK(!other.loadState(saved,problem));}
void stateWaiting() {Chip8 c;load(c,{0xF00A,0x1200});run(c,1);c.setKey(4,true);auto saved=c.saveState();std::string problem;CHECK(c.loadState(saved,problem));CHECK(c.waitingForKey()&&!c.isKeyPressed(4));c.setKey(7,true);c.setKey(7,false);CHECK(!c.waitingForKey()&&c.registers()[0]==7);}
void schedulerChunking() {Chip8 a,b;load(a,{0x7001,0x1200});load(b,{0x7001,0x1200});chip8::Scheduler x(700),y(700);int framesX=0,framesY=0;for(int n=0;n<1000;++n)x.advance(a,0.001,[&]{++framesX;});for(int n=0;n<100;++n)y.advance(b,0.01,[&]{++framesY;});CHECK(framesX==60&&framesY==60&&a.instructionCount()==700);CHECK(a.saveState()==b.saveState());}
void schedulerSafety() {Chip8 c;load(c,{0x7001,0x1200});chip8::Scheduler s(6000);s.advance(c,100);CHECK(c.instructionCount()==300);s.advance(c,std::nan(""));s.advance(c,-1);CHECK(c.instructionCount()==300);s.setHz(0);CHECK(s.hz()==60);}
void presentation() {
    Chip8 c;load(c,{0xA300,0xD011,0xD011,0x1200});CHECK(c.poke(0x300,0x80));
    chip8::Presenter p;CHECK(p.publish(c,true));run(c,2);CHECK(p.publish(c,true));
    CHECK(p.display()[0]==1);run(c,1);
    for(int frame=0;frame<3;++frame)CHECK(!p.publish(c,true)&&p.display()[0]==1);
    CHECK(p.publish(c,true)&&p.display()[0]==0);CHECK(!p.publish(c,true));
}
void presentationSprites() {
    Chip8 c;load(c,{0xA300,0x6205,0xD011,0xD231,0xD011,0xD231,
                   0x7001,0xD011,0x7201,0xD231});
    CHECK(c.poke(0x300,0x80));chip8::Presenter p;p.publish(c,true);
    auto steps=[&](int n){while(n-->0){run(c,1);p.observe(c);}};
    steps(4);p.publish(c,true);CHECK(p.display()[0]==1&&p.display()[5]==1);
    steps(3);CHECK(!pixel(c,0,0)&&!pixel(c,5,0));p.publish(c,true);
    CHECK(p.display()[0]==1&&p.display()[5]==1);
    steps(1);p.publish(c,true); // A moves; B is still being computed.
    CHECK(p.display()[0]==0&&p.display()[1]==1&&p.display()[5]==1);
    steps(2);p.publish(c,true);
    CHECK(p.display()[5]==0&&p.display()[6]==1&&p.display()[1]==1);
    CHECK(p.display()==c.display()); // no old positions accumulate
}
void presentationSameRegisters() {
    Chip8 c;load(c,{0xA300,0xD011,0x7005,0xD011,0xD011,0x60FF,
                   0x6000,0xD011,0xD011,0x6005,0xD011});
    CHECK(c.poke(0x300,0x80));chip8::Presenter p;p.publish(c,true);
    auto steps=[&](int n){while(n-->0){run(c,1);p.observe(c);}};
    steps(4);p.publish(c,true);steps(4);p.publish(c,true);
    CHECK(p.display()[0]==1&&p.display()[5]==1);
    steps(1);p.publish(c,true); // an object is redrawn out of erasure order
    CHECK(p.display()[0]==1&&p.display()[5]==1);
    steps(2);p.publish(c,true);CHECK(p.display()==c.display());
}
void presentationResets() {
    Chip8 c(Config::preset(Profile::SuperChip));
    load(c,{0xA300,0xD011,0xD011,0x00E0,0xD011,0xD011,0x00FB,
            0xD011,0xD011,0x00FF});CHECK(c.poke(0x300,0x80));
    chip8::Presenter p;p.publish(c,true);
    auto steps=[&](int n){while(n-->0){run(c,1);p.observe(c);}};
    steps(2);p.publish(c,true);steps(1);p.publish(c,true);CHECK(p.display()[0]==1);
    steps(1);p.publish(c,true);CHECK(p.display()==c.display()); // clear
    steps(2);p.publish(c,true);CHECK(p.display()[0]==1);
    steps(1);p.publish(c,true);CHECK(p.display()==c.display()); // scroll
    steps(2);p.publish(c,true);CHECK(p.display()[0]==1);
    steps(1);p.publish(c,true);CHECK(p.width()==128&&p.display()==c.display());
    p.reset();p.publish(c,true);CHECK(p.display()==c.display());
}
void presentationPlanes() {
    Chip8 c(Config::preset(Profile::XoChip));
    load(c,{0xA300,0xF301,0xD011,0xD011,0x7001,0xD011});
    CHECK(c.poke(0x300,0x80));CHECK(c.poke(0x301,0xC0));
    chip8::Presenter p;p.publish(c,true);
    auto steps=[&](int n){while(n-->0){run(c,1);p.observe(c);}};
    steps(3);p.publish(c,true);CHECK(p.display()[0]==3&&p.display()[1]==2);
    steps(1);p.publish(c,true);CHECK(p.display()[0]==3&&p.display()[1]==2);
    steps(2);p.publish(c,true);CHECK(p.display()[0]==0&&p.display()[1]==3&&p.display()[2]==2);
    CHECK(p.display()==c.display());
}
void presentationBypass() {
    Chip8 c;load(c,{0xA300,0xD011,0xD011,0xD011,0xD011});CHECK(c.poke(0x300,0x80));
    chip8::Presenter p;p.publish(c,true);run(c,2);p.publish(c,true);
    run(c,1);p.observe(c);p.publish(c,true);CHECK(p.display()[0]==1);
    p.publish(c,false);CHECK(p.display()==c.display());
    p.publish(c,true);CHECK(p.display()==c.display());
    run(c,1);p.observe(c);p.publish(c,true);run(c,1);p.observe(c);
    p.publish(c,true,true);CHECK(p.display()==c.display());
}
void presentationGames() {
    for(int game=0;game<2;++game) {
        Chip8 c,exact;const auto& bytes=game?chip8_demo::Breakout:chip8_demo::PongClassic;
        CHECK(c.loadRom(bytes)&&exact.loadRom(bytes));
        chip8::Scheduler s(700),rawScheduler(700);chip8::Presenter p;p.publish(c,true);
        int expected=-1,checked=0,rawMissing=0,presentMissing=0;
        for(int frame=0;frame<1200;++frame) {
            s.advance(c,1.0/60.0,[&]{
                p.publish(c,true);
                if(expected>=0&&c.pc()>=0x246&&c.pc()<=(game?0x2AA:0x2D4)&&c.delayTimer()==0
                   &&(!game||(expected/128>=18&&expected/128<31))) {
                    ++checked;rawMissing+=c.display()[expected]==0;presentMissing+=p.display()[expected]==0;
                }
            },[&]{
                if(c.spriteUpdate().opcode==0xD671&&c.displayUpdate()==chip8::DisplayUpdate::DrawAdd) {
                    const auto& v=c.registers();expected=(v[7]%c.height())*128+v[6]%c.width();
                }
                p.observe(c);
            });
            rawScheduler.advance(exact,1.0/60.0);
            CHECK(c.saveState()==exact.saveState()); // presentation cannot change the game
        }
        CHECK(checked>100&&rawMissing>50&&presentMissing==0);
        std::cout<<"  "<<(game?"Breakout":"Pong")<<": "<<checked<<" active ball samples, "
                 <<rawMissing<<" XOR gaps, "<<presentMissing<<" presented gaps\n";
    }
}
void originalGames() {for(const auto* bytes:{&chip8_demo::Pong2,&chip8_demo::Demo,&chip8_demo::PongClassic,&chip8_demo::Breakout}) {Chip8 c;CHECK(c.loadRom(*bytes));chip8::Scheduler s(bytes==&chip8_demo::Pong2?6000:700);for(int n=0;n<600;++n)s.advance(c,1.0/60.0);CHECK(!c.halted());CHECK(std::any_of(c.display().begin(),c.display().end(),[](auto b){return b!=0;}));}}
void pongInput() {
    Chip8 c;CHECK(c.loadRom(chip8_demo::Pong2));chip8::Scheduler s(6000);
    auto frames=[&](int count){while(count-->0){s.advance(c,1.0/60.0);CHECK(!c.halted());}};
    frames(60);CHECK(c.registers()[1]==13&&c.registers()[3]==13);
    c.setKey(1,true);frames(60);CHECK(c.registers()[1]==6&&c.registers()[3]==13);
    c.releaseKeys();c.setKey(4,true);c.setKey(12,true);frames(90);
    CHECK(c.registers()[1]==27&&c.registers()[3]==6);
    c.releaseKeys();frames(300);CHECK(c.registers()[1]==27&&c.registers()[3]==6);
    CHECK(c.registers()[4]<=63&&c.registers()[5]>=6&&c.registers()[5]<=31);
    CHECK(c.registers()[8]<16&&c.registers()[9]<16);
}
void fuzz() {
    std::mt19937 random(0xC8);
    for(auto p:{Profile::Vip,Profile::Modern,Profile::SuperChip,Profile::XoChip})for(int trial=0;trial<200;++trial) {
        Chip8 c(Config::preset(p));std::vector<std::uint8_t> bytes(3584);
        for(auto& b:bytes)b=static_cast<std::uint8_t>(random());
        CHECK(c.loadRom(bytes));
        for(int step=0;step<400&&!c.halted();++step) {if(step%8==0)c.tickTimers();c.setKey(static_cast<std::uint8_t>(random()%16),(random()&1)!=0);c.step();}
        const auto saved=c.saveState();std::string problem;auto corrupt=saved;corrupt[random()%corrupt.size()]^=1;CHECK(!c.loadState(corrupt,problem));
    }
}
}
int main() {
    const std::vector<std::pair<const char*,void(*)()>> tests{
        {"Reset/font/generation",resetLoad},{"Atomic ROM rejection",atomicRom},{"Exhaustive VF arithmetic",exhaustiveMath},
        {"Shift source/VF aliases",shifts},{"Logic reset/VF operand",logic},{"7XNN preserves VF",addNoFlag},
        {"Call/return",callStack},{"Stack bounds",stackBounds},{"Conditional skips",skips},{"BNNN profiles",jumpQuirk},
        {"RAM bounds/atomic writes",bounds},{"Invalid opcodes",invalidOps},{"BCD all byte values",bcd},{"Load/store profiles",memoryQuirk},
        {"Fonts/masked digits",fonts},{"Timers saturate",timers},{"Key skip/down/up",keys},{"FX0A release/repeat",keyWait},
        {"FX0A held key",heldKey},{"Focus loss/re-arm",focusKeys},{"Sprite XOR/collision",drawCollision},{"VF draw coordinates",drawVfCoordinates},
        {"Clipping/wrapping",clipWrap},{"Vertical blank gate",vblank},{"Resolution/16x16",resolutions},{"SCHIP scrolling",scroll},
        {"RPL flags",flags},{"XO long skip",xoLongSkip},{"XO 64K memory",xoMemory},{"XO range reverse",xoRange},
        {"XO selected planes",xoPlanes},{"XO pattern/pitch",xoAudio},{"State deterministic replay",stateRoundtrip},
        {"State corruption/ROM binding",stateValidation},{"State input re-arm",stateWaiting},{"Scheduler chunk invariance",schedulerChunking},
        {"Scheduler host stall bounds",schedulerSafety},{"Presentation finite erasure hold",presentation},
        {"Presentation independent sprites/no trails",presentationSprites},{"Presentation shared-register pairing",presentationSameRegisters},
        {"Presentation clear/scroll/mode reset",presentationResets},{"Presentation XO planes",presentationPlanes},
        {"Presentation exact/forced bypass",presentationBypass},{"Pong/Breakout flicker and state regression",presentationGames},
        {"Bundled ROM smoke",originalGames},{"Two-player Pong input/no AI",pongInput},
        {"Random ROM/state fuzz",fuzz}
    };
    int passed=0;
    for(const auto& test:tests)try {test.second();++passed;std::cout<<"PASS "<<test.first<<"\n";}catch(const std::exception& e) {std::cerr<<"FAIL "<<test.first<<": "<<e.what()<<"\n";return 1;}
    std::cout<<"All "<<passed<<" test groups passed.\n";
}
