#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <mmsystem.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <xinput.h>

#include "Chip8Core.hpp"
#include "DemoRom.hpp"
#include "resource.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#ifdef _MSC_VER
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "Gdiplus.lib")
#pragma comment(lib, "Ole32.lib")
#endif

namespace {
using Clock = std::chrono::steady_clock;
namespace fs = std::filesystem;
enum : UINT {
    Open = 1001, Demo, Restart, Folder, Exit, Pong2, PongClassic, Breakout,
    Pause, Save, Load, Screenshot, Rewind, Step, FrameStep, Debug, Fullscreen, Mute,
    About, Help, Profile0 = 1100, Speed0 = 1200, Palette0 = 1300, Quirk0 = 1400,
    Stable = 1500, Exact, AutoPause, Gamepad
};
constexpr std::array<int, 7> Speeds{420,700,1000,1500,3000,6000,12000};
constexpr wchar_t ClassName[] = L"CHIP8StudioWindowV2";
struct Palette { const wchar_t* name; std::array<std::uint32_t,4> color; };
constexpr std::array<Palette,4> Palettes{{
    {L"Лёд", {0x0B1420,0xD8F3FF,0x66B3FF,0xFFBD86}},
    {L"Зелёный", {0x07150E,0x98F5AD,0x49B889,0xFCED96}},
    {L"Янтарный", {0x1B1206,0xFFD080,0xF0784B,0xFFFFCA}},
    {L"Классический", {0x050505,0xF0F0F0,0x777777,0xFFFFFF}}
}};

chip8::Chip8 cpu;
chip8::Scheduler scheduler;
chip8::Presenter presenter;
HWND window = nullptr;
HMENU menu = nullptr;
HFONT font = nullptr, titleFont = nullptr, monoFont = nullptr;
UINT dpi = 96;
bool paused = false, active = true, minimized = false, modal = false;
bool debug = false, fullscreen = false, muted = false, stable = true;
bool autoPause = true, gamepad = true, clockReset = false, closing = false;
bool pongControls = false;
int palette = 0;
std::wstring romName, notice;
Clock::time_point noticeUntil{};
fs::path exeDir, dataDir, settings, romPath;
std::vector<std::uint8_t> romBytes;
std::array<bool,256> physicalKeys{};
std::deque<std::vector<std::uint8_t>> history;
unsigned historyFrames = 0;
WINDOWPLACEMENT placement{};
DWORD oldStyle = 0;
ULONG_PTR gdiplusToken = 0;

std::wstring wide(const std::string& text) {
    if(text.empty())return {};
    const int count=MultiByteToWideChar(CP_UTF8,0,text.data(),static_cast<int>(text.size()),nullptr,0);
    std::wstring out(static_cast<std::size_t>(count),0);
    MultiByteToWideChar(CP_UTF8,0,text.data(),static_cast<int>(text.size()),out.data(),count);
    return out;
}
template<class T> T procedure(HMODULE module,const char* name) {
    const auto address=GetProcAddress(module,name);
    T result=nullptr;
    static_assert(sizeof(result)==sizeof(address));
    std::memcpy(&result,&address,sizeof(result));
    return result;
}
std::wstring hex(std::uint64_t value, int width = 4) {
    std::wostringstream out;
    out<<std::hex<<std::uppercase<<std::setfill(L'0')<<std::setw(width)<<value;
    return out.str();
}
int ui(int value) { return MulDiv(value,static_cast<int>(dpi),96); }
COLORREF color(std::uint32_t rgb) { return RGB((rgb>>16)&255,(rgb>>8)&255,rgb&255); }
void toast(const std::wstring& text) { notice=text; noticeUntil=Clock::now()+std::chrono::seconds(4); }
void redraw() { if(window) { InvalidateRect(window,nullptr,FALSE); UpdateWindow(window); } }
void error(const std::wstring& text) {
    modal=true; cpu.releaseKeys(); physicalKeys.fill(false);
    MessageBoxW(window,text.c_str(),L"CHIP-8 Studio",MB_OK|MB_ICONERROR);
    modal=false; clockReset=true;
}
bool running() { return !paused && !minimized && !modal && (!autoPause || active) && !cpu.halted(); }
std::wstring gameId() { return hex(cpu.romHash(),16); }
fs::path statePath() { return dataDir / (gameId()+L".c8state"); }

class BackBuffer {
public:
    BackBuffer() = default;
    BackBuffer(const BackBuffer&) = delete;
    BackBuffer& operator=(const BackBuffer&) = delete;
    ~BackBuffer() { clear(); }
    bool ensure(HDC ref,int w,int h) {
        if(w<=0||h<=0)return false;
        if(dc_&&w==width_&&h==height_)return true;
        clear();
        dc_=CreateCompatibleDC(ref);
        if(!dc_)return false;
        BITMAPINFO info{};
        info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=w; info.bmiHeader.biHeight=-h;
        info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32;
        info.bmiHeader.biCompression=BI_RGB;
        bitmap_=CreateDIBSection(ref,&info,DIB_RGB_COLORS,&bits_,nullptr,0);
        if(!bitmap_||!bits_) { clear(); return false; }
        old_=SelectObject(dc_,bitmap_);
        if(!old_||old_==HGDI_ERROR) { old_=nullptr; clear(); return false; }
        width_=w; height_=h;
        return true;
    }
    void clear() {
        if(dc_&&old_)SelectObject(dc_,old_);
        if(bitmap_)DeleteObject(bitmap_);
        if(dc_)DeleteDC(dc_);
        dc_=nullptr; bitmap_=nullptr; old_=nullptr; bits_=nullptr;
        width_=height_=0;
    }
    HDC dc()const{return dc_;}
    std::uint32_t* pixels()const{return static_cast<std::uint32_t*>(bits_);}
    int width()const{return width_;}
    int height()const{return height_;}
private:
    HDC dc_=nullptr;
    HBITMAP bitmap_=nullptr;
    HGDIOBJ old_=nullptr;
    void* bits_=nullptr;
    int width_=0,height_=0;
} back;

class Audio {
public:
    bool start() {
        WAVEFORMATEX format{};
        format.wFormatTag=WAVE_FORMAT_PCM; format.nChannels=1;
        format.nSamplesPerSec=48000; format.wBitsPerSample=16;
        format.nBlockAlign=2; format.nAvgBytesPerSec=96000;
        if(waveOutOpen(&device_,WAVE_MAPPER,&format,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR) { device_=nullptr; return false; }
        for(std::size_t i=0;i<headers_.size();++i) {
            headers_[i].lpData=reinterpret_cast<LPSTR>(samples_[i].data());
            headers_[i].dwBufferLength=static_cast<DWORD>(samples_[i].size()*sizeof(std::int16_t));
            if(waveOutPrepareHeader(device_,&headers_[i],sizeof(WAVEHDR))!=MMSYSERR_NOERROR) { stop(); return false; }
            headers_[i].dwFlags|=WHDR_DONE;
        }
        return true;
    }
    void pump(bool enabled) {
        if(!device_)return;
        const bool on=enabled&&cpu.soundTimer()>0;
        if(!on)phase_=0;
        const bool xo=cpu.config().xo();
        const double frequency=xo?4000.0*std::pow(2.0,(static_cast<int>(cpu.pitch())-64)/48.0):440.0;
        for(std::size_t i=0;i<headers_.size();++i) {
            auto& header=headers_[i];
            if(!(header.dwFlags&WHDR_DONE))continue;
            for(auto& sample:samples_[i]) {
                gain_+=(on?0.11-gain_:-gain_)*0.01;
                double value=0;
                if(on) {
                    if(xo) {
                        const auto index=static_cast<unsigned>(phase_)&127u;
                        value=(cpu.audioPattern()[index/8]&(0x80>>(index&7)))?1.0:-1.0;
                        phase_+=frequency/48000.0;
                        if(phase_>=128.0)phase_=std::fmod(phase_,128.0);
                    } else {
                        value=std::sin(phase_*6.283185307179586);
                        phase_+=frequency/48000.0;
                        if(phase_>=1.0)phase_-=1.0;
                    }
                }
                sample=static_cast<std::int16_t>(value*gain_*32767.0);
            }
            if(waveOutWrite(device_,&header,sizeof(WAVEHDR))!=MMSYSERR_NOERROR) { stop(); return; }
        }
    }
    void silence() { if(device_)waveOutReset(device_); phase_=gain_=0; }
    void stop() {
        if(!device_)return;
        waveOutReset(device_);
        for(auto& h:headers_)if(h.dwFlags&WHDR_PREPARED)waveOutUnprepareHeader(device_,&h,sizeof(WAVEHDR));
        waveOutClose(device_); device_=nullptr;
    }
    ~Audio(){stop();}
private:
    HWAVEOUT device_=nullptr;
    std::array<std::array<std::int16_t,256>,4> samples_{};
    std::array<WAVEHDR,4> headers_{};
    double phase_=0,gain_=0;
} audio;

class Pads {
public:
    void init() {
        for(const auto* name:{L"xinput1_4.dll",L"xinput9_1_0.dll"}) {
            library_=LoadLibraryExW(name,nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
            if(library_)break;
        }
        if(library_)get_=procedure<Get>(library_,"XInputGetState");
    }
    void add(std::array<bool,16>& keys) {
        if(!get_||!gamepad||!active||modal)return;
        for(DWORD index=0;index<2;++index) {
            XINPUT_STATE state{};
            if(get_(index,&state)!=ERROR_SUCCESS)continue;
            const auto buttons=state.Gamepad.wButtons;
            const bool up=(buttons&XINPUT_GAMEPAD_DPAD_UP)||state.Gamepad.sThumbLY>16000;
            const bool down=(buttons&XINPUT_GAMEPAD_DPAD_DOWN)||state.Gamepad.sThumbLY<-16000;
            if(pongControls) {
                keys[index?12:1]=keys[index?12:1]||up;
                keys[index?13:4]=keys[index?13:4]||down;
            } else {
                keys[5]=keys[5]||up; keys[8]=keys[8]||down;
                keys[7]=keys[7]||(buttons&XINPUT_GAMEPAD_DPAD_LEFT)||state.Gamepad.sThumbLX<-16000;
                keys[9]=keys[9]||(buttons&XINPUT_GAMEPAD_DPAD_RIGHT)||state.Gamepad.sThumbLX>16000;
                for(const auto& pair:{std::pair<WORD,int>{XINPUT_GAMEPAD_A,6},{XINPUT_GAMEPAD_B,4},{XINPUT_GAMEPAD_X,0},{XINPUT_GAMEPAD_Y,15},{XINPUT_GAMEPAD_START,10}})
                    keys[pair.second]=keys[pair.second]||(buttons&pair.first);
            }
        }
    }
    ~Pads(){if(library_)FreeLibrary(library_);}
private:
    using Get=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);
    HMODULE library_=nullptr;
    Get get_=nullptr;
} pads;

fs::path executableDirectory() {
    std::wstring buffer(32768,L'\0');
    const DWORD n=GetModuleFileNameW(nullptr,buffer.data(),static_cast<DWORD>(buffer.size()));
    if(!n||n>=buffer.size())return L".";
    buffer.resize(n); return fs::path(buffer).parent_path();
}
void initializeData() {
    wchar_t path[MAX_PATH]{};
    if(SUCCEEDED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,path)))
        dataDir=fs::path(path)/L"CHIP8Studio";
    else dataDir=exeDir/L"userdata";
    std::error_code ec;
    fs::create_directories(dataDir,ec);
    settings=dataDir/L"settings.ini";
    palette=std::clamp<int>(GetPrivateProfileIntW(L"App",L"Palette",0,settings.c_str()),0,3);
    muted=GetPrivateProfileIntW(L"App",L"Mute",0,settings.c_str())!=0;
    stable=GetPrivateProfileIntW(L"App",L"Stable",1,settings.c_str())!=0;
    autoPause=GetPrivateProfileIntW(L"App",L"AutoPause",1,settings.c_str())!=0;
    gamepad=GetPrivateProfileIntW(L"App",L"Gamepad",1,settings.c_str())!=0;
}
bool writeSetting(const wchar_t* section,const wchar_t* key,const std::wstring& value) {
    return WritePrivateProfileStringW(section,key,value.c_str(),settings.c_str())!=FALSE;
}
void persistSettings() {
    if(!cpu.romHash())return;
    const auto id=gameId();
    bool ok=writeSetting(id.c_str(),L"Profile",std::to_wstring(static_cast<int>(cpu.config().profile)));
    ok=writeSetting(id.c_str(),L"Quirks",std::to_wstring(cpu.config().quirks()))&&ok;
    ok=writeSetting(id.c_str(),L"Hz",std::to_wstring(static_cast<int>(scheduler.hz())))&&ok;
    ok=writeSetting(L"App",L"Palette",std::to_wstring(palette))&&ok;
    ok=writeSetting(L"App",L"Mute",std::to_wstring(muted))&&ok;
    ok=writeSetting(L"App",L"Stable",std::to_wstring(stable))&&ok;
    ok=writeSetting(L"App",L"AutoPause",std::to_wstring(autoPause))&&ok;
    ok=writeSetting(L"App",L"Gamepad",std::to_wstring(gamepad))&&ok;
    if(!ok)toast(L"Не удалось записать настройки. Проверь доступ к папке данных.");
}
bool readFile(const fs::path& path,std::vector<std::uint8_t>& bytes,std::size_t maxBytes) {
    std::ifstream stream(path,std::ios::binary|std::ios::ate);
    if(!stream)return false;
    const auto size=stream.tellg();
    if(size<0||static_cast<std::uint64_t>(size)>maxBytes)return false;
    stream.seekg(0);
    std::vector<std::uint8_t> result(static_cast<std::size_t>(size));
    if(size>0&&!stream.read(reinterpret_cast<char*>(result.data()),size))return false;
    bytes.swap(result); return true;
}
bool writeAtomic(const fs::path& path,const std::vector<std::uint8_t>& bytes) {
    const fs::path temp=path.wstring()+L".tmp";
    {
        std::ofstream stream(temp,std::ios::binary|std::ios::trunc);
        if(!stream)return false;
        stream.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
        stream.flush();
        if(!stream)return false;
    }
    if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        std::error_code ec; fs::remove(temp,ec); return false;
    }
    return true;
}
void saveFlags() {
    if(!cpu.romHash()||!cpu.config().extended())return;
    const auto& flags=cpu.flags();
    writeAtomic(dataDir/(gameId()+L".rpl"),std::vector<std::uint8_t>(flags.begin(),flags.end()));
}
void restoreFlags(chip8::Chip8& next) {
    std::vector<std::uint8_t> bytes;
    if(readFile(dataDir/(hex(next.romHash(),16)+L".rpl"),bytes,16)&&bytes.size()==16) {
        std::array<std::uint8_t,16> flags{}; std::copy(bytes.begin(),bytes.end(),flags.begin()); next.setFlags(flags);
    }
}
void updateMenu() {
    if(!menu)return;
    auto check=[](UINT id,bool on){CheckMenuItem(menu,id,MF_BYCOMMAND|(on?MF_CHECKED:MF_UNCHECKED));};
    check(Pause,paused); check(Mute,muted); check(Debug,debug); check(Fullscreen,fullscreen);
    check(Stable,stable); check(Exact,!stable); check(AutoPause,autoPause); check(Gamepad,gamepad);
    for(unsigned n=0;n<4;++n) { check(Profile0+n,static_cast<unsigned>(cpu.config().profile)==n); check(Palette0+n,palette==static_cast<int>(n)); }
    for(unsigned n=0;n<Speeds.size();++n)check(Speed0+n,std::abs(scheduler.hz()-Speeds[n])<0.5);
    for(unsigned n=0;n<7;++n)check(Quirk0+n,(cpu.config().quirks()&(1u<<n))!=0);
    SetWindowTextW(window,(L"CHIP-8 Studio — "+romName+(paused?L" [ПАУЗА]":L"")).c_str());
}
void resetHistory() { history.clear(); history.push_back(cpu.saveState()); historyFrames=0; }
void refresh(bool force = true) {
    presenter.publish(cpu,stable,force); updateMenu(); redraw();
}
bool commitGame(const std::vector<std::uint8_t>& bytes,const std::wstring& name,
                chip8::Config config,int defaultHz,const fs::path& path={},bool remember=true,bool useSaved=true) {
    chip8::Chip8 next(chip8::Config::preset(chip8::Profile::XoChip));
    if(!next.loadRom(bytes)) { error(L"ROM должен содержать от 2 до 65024 байт."); return false; }
    const auto id=hex(next.romHash(),16);
    int hz=defaultHz;
    if(useSaved) {
        const int p=std::clamp<int>(GetPrivateProfileIntW(id.c_str(),L"Profile",static_cast<int>(config.profile),settings.c_str()),0,3);
        config=chip8::Config::preset(static_cast<chip8::Profile>(p));
        const int q=GetPrivateProfileIntW(id.c_str(),L"Quirks",config.quirks(),settings.c_str());
        if(q>=0&&q<=127)config=chip8::Config::fromQuirks(config.profile,static_cast<std::uint8_t>(q));
        hz=std::clamp<int>(GetPrivateProfileIntW(id.c_str(),L"Hz",hz,settings.c_str()),60,100000);
    }
    next.setConfig(config);
    if(!next.loadRom(bytes)) { error(L"Этот ROM слишком велик для выбранного профиля. Выбери XO-CHIP."); return false; }
    if(remember) { persistSettings(); saveFlags(); }
    restoreFlags(next);
    next.seedRandom(static_cast<std::uint32_t>(Clock::now().time_since_epoch().count()));
    cpu=std::move(next); romBytes=bytes; romName=name; romPath=path;
    pongControls=(bytes==chip8_demo::Pong2);
    scheduler.setHz(hz); scheduler.reset(); presenter.reset(); resetHistory();
    physicalKeys.fill(false); paused=false; clockReset=true; audio.silence(); notice.clear();
    refresh();
    return true;
}
bool loadPath(const fs::path& path) {
    std::vector<std::uint8_t> bytes;
    if(!readFile(path,bytes,65024)||bytes.size()<2) { error(L"Не удалось прочитать ROM (2–65024 байт):\n"+path.wstring()); return false; }
    const auto ext=path.extension().wstring();
    auto profile=bytes.size()>3584||ext==L".xo8"?chip8::Profile::XoChip:ext==L".sc8"?chip8::Profile::SuperChip:chip8::Profile::Modern;
    const int hz=profile==chip8::Profile::XoChip?6000:profile==chip8::Profile::SuperChip?1500:700;
    if(!commitGame(bytes,path.filename().wstring(),chip8::Config::preset(profile),hz,path))return false;
    writeSetting(L"App",L"LastFolder",path.parent_path().wstring());
    return true;
}
void openFile() {
    std::wstring file(32768,L'\0');
    wchar_t folder[32768]{};
    GetPrivateProfileStringW(L"App",L"LastFolder",(exeDir/L"roms").c_str(),folder,32768,settings.c_str());
    OPENFILENAMEW dialog{}; dialog.lStructSize=sizeof(dialog); dialog.hwndOwner=window;
    dialog.lpstrFilter=L"CHIP-8 / Super-CHIP / XO-CHIP\0*.ch8;*.rom;*.sc8;*.xo8\0Все файлы\0*.*\0";
    dialog.lpstrFile=file.data(); dialog.nMaxFile=static_cast<DWORD>(file.size());
    dialog.lpstrInitialDir=folder; dialog.lpstrTitle=L"Открыть ROM";
    dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    modal=true; cpu.releaseKeys(); physicalKeys.fill(false); audio.silence();
    const bool selected=GetOpenFileNameW(&dialog)!=FALSE;
    modal=false; clockReset=true;
    if(selected)loadPath(fs::path(file.c_str()));
}
void restart() {
    if(romBytes.empty())return;
    const auto bytes=romBytes; const auto name=romName; const auto path=romPath;
    commitGame(bytes,name,cpu.config(),static_cast<int>(scheduler.hz()),path,true,false);
}
void changeConfig(chip8::Config config) {
    const auto bytes=romBytes; const auto name=romName; const auto path=romPath;
    const int hz=config.profile==cpu.config().profile?static_cast<int>(scheduler.hz()):
        config.xo()?6000:config.profile==chip8::Profile::SuperChip?1500:pongControls?6000:700;
    if(commitGame(bytes,name,config,hz,path,true,false)) {
        persistSettings(); toast(L"Профиль применён. ROM перезапущен.");
    }
}
void saveSlot() {
    if(writeAtomic(statePath(),cpu.saveState()))toast(L"Состояние сохранено · F8 — загрузить");
    else error(L"Не удалось сохранить состояние:\n"+statePath().wstring());
}
bool loadSlot() {
    std::vector<std::uint8_t> bytes;
    if(!readFile(statePath(),bytes,80000)) { error(L"Сохранение не найдено для этого ROM. Нажми F5 для создания."); return false; }
    std::string problem;
    if(!cpu.loadState(bytes,problem)) { error(L"Не удалось загрузить сохранение:\n"+wide(problem)); return false; }
    cpu.releaseKeys(); physicalKeys.fill(false); scheduler.reset(); clockReset=true;
    audio.silence(); presenter.reset(); resetHistory(); persistSettings(); refresh(); toast(L"Состояние восстановлено");
    return true;
}
void rewind() {
    if(history.empty())return;
    if(history.size()>1)history.pop_back();
    std::string problem;
    if(cpu.loadState(history.back(),problem)) {
        paused=true; cpu.releaseKeys(); physicalKeys.fill(false); historyFrames=0;
        scheduler.reset(); clockReset=true; presenter.reset(); audio.silence(); refresh();
        toast(L"Назад примерно на секунду · P — продолжить");
    }
}
void onFrame() {
    if(presenter.publish(cpu,stable))redraw();
    if(++historyFrames>=60) {
        historyFrames=0;
        history.push_back(cpu.saveState());
        if(history.size()>31)history.pop_front();
    }
}

void onDisplay() { if(stable)presenter.observe(cpu); }

bool pngEncoder(CLSID& id) {
    UINT count=0,size=0;
    if(Gdiplus::GetImageEncodersSize(&count,&size)!=Gdiplus::Ok||!size)return false;
    std::vector<std::uint8_t> buffer(size);
    auto* encoders=reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
    if(Gdiplus::GetImageEncoders(count,size,encoders)!=Gdiplus::Ok)return false;
    for(UINT n=0;n<count;++n)if(wcscmp(encoders[n].MimeType,L"image/png")==0) { id=encoders[n].Clsid; return true; }
    return false;
}
bool exportScreenshot(const fs::path& path) {
    CLSID encoder{};
    if(!pngEncoder(encoder))return false;
    const int zoom=presenter.width()==128?4:8,w=presenter.width()*zoom,h=presenter.height()*zoom;
    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(w)*h);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x)
        pixels[static_cast<std::size_t>(y)*w+x]=0xFF000000u|Palettes[palette].color[presenter.display()[static_cast<std::size_t>(y/zoom)*128+x/zoom]&3];
    Gdiplus::Bitmap image(w,h,w*4,PixelFormat32bppARGB,reinterpret_cast<BYTE*>(pixels.data()));
    return image.Save(path.c_str(),&encoder,nullptr)==Gdiplus::Ok;
}
void screenshot() {
    SYSTEMTIME time{}; GetLocalTime(&time);
    const auto name=L"CHIP8-"+gameId()+L"-"+std::to_wstring(time.wYear)+L"-"+std::to_wstring(time.wMonth)+L"-"+
        std::to_wstring(time.wDay)+L"-"+std::to_wstring(time.wHour)+L"-"+std::to_wstring(time.wMinute)+L"-"+std::to_wstring(time.wSecond)+L".png";
    if(exportScreenshot(dataDir/name))toast(L"PNG сохранён. Файл → Папка данных.");
    else error(L"Не удалось сохранить PNG.");
}
void toggleFullscreen() {
    if(!fullscreen) {
        placement.length=sizeof(placement); GetWindowPlacement(window,&placement);
        oldStyle=static_cast<DWORD>(GetWindowLongPtrW(window,GWL_STYLE));
        MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
        if(!GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor))return;
        SetMenu(window,nullptr);
        SetWindowLongPtrW(window,GWL_STYLE,oldStyle&~static_cast<DWORD>(WS_OVERLAPPEDWINDOW));
        SetWindowPos(window,HWND_TOP,monitor.rcMonitor.left,monitor.rcMonitor.top,
            monitor.rcMonitor.right-monitor.rcMonitor.left,monitor.rcMonitor.bottom-monitor.rcMonitor.top,
            SWP_NOOWNERZORDER|SWP_FRAMECHANGED);
        fullscreen=true;
    } else {
        fullscreen=false;
        SetWindowLongPtrW(window,GWL_STYLE,oldStyle); SetMenu(window,menu);
        SetWindowPlacement(window,&placement);
        SetWindowPos(window,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_FRAMECHANGED);
    }
    clockReset=true; updateMenu(); redraw();
}
void createFonts() {
    for(auto f:{font,titleFont,monoFont})if(f)DeleteObject(f);
    font=CreateFontW(-ui(14),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    titleFont=CreateFontW(-ui(23),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    monoFont=CreateFontW(-ui(13),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,FIXED_PITCH,L"Consolas");
}
void text(HDC dc,const std::wstring& value,RECT rect,std::uint32_t rgb,HFONT face=nullptr,UINT flags=DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS) {
    const auto old=SelectObject(dc,face?face:font);
    SetBkMode(dc,TRANSPARENT); SetTextColor(dc,color(rgb));
    DrawTextW(dc,value.c_str(),static_cast<int>(value.size()),&rect,flags);
    SelectObject(dc,old);
}
void fill(HDC dc,RECT rect,std::uint32_t rgb) {
    const auto brush=CreateSolidBrush(color(rgb)); FillRect(dc,&rect,brush); DeleteObject(brush);
}
constexpr std::array<UINT,6> ToolbarActions{Open,Pong2,Pause,Restart,Save,Debug};
RECT buttonRect(int index) { return {ui(20+index*110),ui(65),ui(122+index*110),ui(96)}; }
void paint(HDC destination) {
    RECT client{}; GetClientRect(window,&client);
    const int w=client.right,h=client.bottom;
    if(!back.ensure(destination,w,h)) { FillRect(destination,&client,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH))); return; }
    std::fill_n(back.pixels(),static_cast<std::size_t>(w)*h,0x101722u);
    RECT area{ui(20),ui(116),w-ui(debug?312:20),h-ui(92)};
    if(fullscreen)area={0,0,w,h};
    const int logicalW=presenter.width(),logicalH=presenter.height();
    const int zoom=std::max(1,static_cast<int>(std::min((area.right-area.left)/logicalW,(area.bottom-area.top)/logicalH)));
    const int originX=area.left+((area.right-area.left)-logicalW*zoom)/2;
    const int originY=area.top+((area.bottom-area.top)-logicalH*zoom)/2;
    // Every output frame is reconstructed; erasures never leave phosphor trails.
    for(int y=0;y<logicalH;++y)for(int x=0;x<logicalW;++x) {
        const auto rgb=Palettes[palette].color[presenter.display()[static_cast<std::size_t>(y)*128+x]&3];
        for(int sy=0;sy<zoom;++sy) {
            const int py=originY+y*zoom+sy,px=originX+x*zoom;
            if(py<0||py>=h||px<0||px+zoom>w)continue;
            std::fill_n(back.pixels()+static_cast<std::size_t>(py)*w+px,zoom,rgb);
        }
    }
    const HDC dc=back.dc();
    if(!fullscreen) {
        fill(dc,{0,0,w,ui(106)},0x182334);
        text(dc,L"CHIP-8 STUDIO",{ui(20),ui(9),ui(244),ui(44)},0xD8F3FF,titleFont);
        text(dc,romName,{ui(265),ui(12),w-ui(20),ui(42)},0xAFC1D5);
        const std::array<std::wstring,6> labels{L"Открыть ROM",L"Pong: 2 игрока",paused?L"Продолжить":L"Пауза",L"Перезапуск",L"Сохранить",debug?L"Скрыть отладку":L"Отладка"};
        for(int n=0;n<6;++n) {
            const auto rect=buttonRect(n); fill(dc,rect,0x27384E);
            text(dc,labels[n],rect,0xD8E7F5,nullptr,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
        }
        fill(dc,{0,h-ui(78),w,h},0x182334);
        std::wstring status=wide(chip8::profileName(cpu.config().profile))+L"   ·   "+std::to_wstring(static_cast<int>(scheduler.hz()))+
            L" инстр/с   ·   "+std::to_wstring(cpu.width())+L"×"+std::to_wstring(cpu.height())+L"   ·   "+(stable?L"Стабилизация XOR":L"Точный экран");
        if(paused)status+=L"   ·   ПАУЗА";
        else if(!running()&&!cpu.halted())status+=L"   ·   Автопауза";
        if(muted)status+=L"   ·   Без звука";
        text(dc,status,{ui(20),h-ui(75),w-ui(20),h-ui(52)},0x9CBAD2);
        const auto controls=pongControls?L"Pong: W/S — слева, ↑/↓ — справа. Два игрока, без ИИ.":L"Клавиатура: 1 2 3 4 / Q W E R / A S D F / Z X C V — работает при любой раскладке.";
        text(dc,controls,{ui(20),h-ui(53),w-ui(20),h-ui(31)},0xD3E5F3);
        std::wstring help=L"P — пауза  ·  F1 — перезапуск  ·  F5/F8 — сохранить/загрузить  ·  F11 — весь экран";
        if(cpu.halted())help=cpu.error().empty()?L"Программа завершила выполнение (00FD). F1 — начать заново.":wide(cpu.error());
        else if(Clock::now()<noticeUntil)help=notice;
        text(dc,help,{ui(20),h-ui(31),w-ui(20),h-ui(6)},cpu.error().empty()?0x7F9DB7:0xFFAD91);
        if(debug) {
            const int left=w-ui(292);
            fill(dc,{left-ui(10),ui(106),w,h-ui(78)},0x141F2E);
            const int savedDc=SaveDC(dc);
            IntersectClipRect(dc,left-ui(10),ui(106),w,h-ui(78));
            int y=ui(119);
            auto line=[&](const std::wstring& str,std::uint32_t rgb=0xAFC1D5){text(dc,str,{left,y,w-ui(12),y+ui(22)},rgb,monoFont);y+=ui(23);};
            line(L"CPU / ПАМЯТЬ",0xD8F3FF);
            line(L"PC "+hex(cpu.pc())+L"    I "+hex(cpu.index()));
            line(L"DT "+hex(cpu.delayTimer(),2)+L"  ST "+hex(cpu.soundTimer(),2)+L"  SP "+hex(cpu.stackDepth(),2));
            line(L"Инструкций: "+std::to_wstring(cpu.instructionCount()));
            if(cpu.waitingForKey())line(L"Ожидание нажатия/отпускания",0xFFD080);
            if(cpu.waitingForDraw())line(L"Ожидание vblank",0xFFD080);
            for(int row=0;row<4;++row) {
                std::wstring value;
                for(int column=0;column<4;++column) { const int n=row*4+column;value+=L"V"+hex(n,1)+L":"+hex(cpu.registers()[n],2)+L"  "; }
                line(value);
            }
            y+=ui(9); line(L"F10: инструкция · F9: кадр",0xD8F3FF);
            line(L"Backspace: назад ~1 секунду");
            y+=ui(9);
            std::uint16_t address=cpu.pc();
            for(int n=0;n<6;++n) {
                const auto decoded=cpu.disassemble(address); line(wide(decoded),n==0?0x98F5AD:0x7F9DB7);
                const auto& mem=cpu.memory();
                const bool longOp=cpu.config().xo()&&static_cast<std::size_t>(address)+1<cpu.memoryLimit()&&mem[address]==0xF0&&mem[address+1]==0;
                address=static_cast<std::uint16_t>(address+(longOp?4:2));
            }
            y+=ui(9); line(L"I: следующие 16 байт");
            for(int row=0;row<2;++row) {
                std::wstring values;
                for(int n=0;n<8;++n) { const auto offset=static_cast<std::size_t>(cpu.index())+row*8+n; values+=(offset<cpu.memoryLimit()?hex(cpu.memory()[offset],2):L"--")+L" "; }
                line(values);
            }
            RestoreDC(dc,savedDc);
        }
    }
    BitBlt(destination,0,0,w,h,dc,0,0,SRCCOPY);
}

HMENU makeMenu() {
    HMENU bar=CreateMenu();
    auto popup=[&](const wchar_t* name){auto m=CreatePopupMenu();AppendMenuW(bar,MF_POPUP,reinterpret_cast<UINT_PTR>(m),name);return m;};
    auto add=[](HMENU m,UINT id,const wchar_t* label){AppendMenuW(m,MF_STRING,id,label);};
    auto file=popup(L"Файл"); add(file,Open,L"Открыть ROM…\tCtrl+O"); add(file,Restart,L"Перезапустить текущий ROM\tF1");
    add(file,Save,L"Сохранить состояние\tF5"); add(file,Load,L"Загрузить состояние\tF8"); add(file,Screenshot,L"Снимок PNG\tF6");
    add(file,Folder,L"Папка данных и снимков"); AppendMenuW(file,MF_SEPARATOR,0,nullptr); add(file,Exit,L"Выход\tAlt+F4");
    auto games=popup(L"Игры"); add(games,Pong2,L"Pong — два игрока (встроен)"); add(games,PongClassic,L"Pong — исходный ROM");
    add(games,Breakout,L"Breakout (встроен)"); add(games,Demo,L"Анимированная демонстрация");
    auto emulation=popup(L"Эмуляция"); add(emulation,Pause,L"Пауза\tP"); add(emulation,Step,L"Одна инструкция\tF10"); add(emulation,FrameStep,L"Один кадр\tF9");
    add(emulation,Rewind,L"Назад примерно на секунду\tBackspace");
    HMENU profiles=CreatePopupMenu();
    for(unsigned n=0;n<4;++n)add(profiles,Profile0+n,wide(chip8::profileName(static_cast<chip8::Profile>(n))).c_str());
    AppendMenuW(emulation,MF_POPUP,reinterpret_cast<UINT_PTR>(profiles),L"Профиль совместимости (перезапуск)");
    HMENU speeds=CreatePopupMenu();
    for(unsigned n=0;n<Speeds.size();++n)add(speeds,Speed0+n,(std::to_wstring(Speeds[n])+L" инструкций/с").c_str());
    AppendMenuW(emulation,MF_POPUP,reinterpret_cast<UINT_PTR>(speeds),L"Скорость CPU");
    HMENU quirks=CreatePopupMenu();
    constexpr const wchar_t* names[]{L"Сдвиг использует VX",L"FX55/FX65 увеличивают I",L"OR/AND/XOR сбрасывают VF",L"BNNN использует VX",L"Обрезать спрайты по краям",L"DXYN ждёт vblank (60 Гц)",L"FX0A ждёт отпускания клавиши"};
    for(unsigned n=0;n<7;++n)add(quirks,Quirk0+n,names[n]);
    AppendMenuW(emulation,MF_POPUP,reinterpret_cast<UINT_PTR>(quirks),L"Особенности инструкций (перезапуск)");
    add(emulation,AutoPause,L"Автопауза без фокуса"); add(emulation,Gamepad,L"Геймпады XInput");
    auto view=popup(L"Экран"); add(view,Stable,L"Стабилизация XOR (по спрайтам)"); add(view,Exact,L"Точное состояние экрана (60 Гц)");
    HMENU colors=CreatePopupMenu(); for(unsigned n=0;n<Palettes.size();++n)add(colors,Palette0+n,Palettes[n].name);
    AppendMenuW(view,MF_POPUP,reinterpret_cast<UINT_PTR>(colors),L"Палитра"); add(view,Fullscreen,L"Полный экран\tF11"); add(view,Debug,L"Панель отладки\tF12"); add(view,Mute,L"Отключить звук\tM");
    auto help=popup(L"Справка"); add(help,Help,L"Управление и совместимость"); add(help,About,L"О программе");
    return bar;
}
int scanToKey(unsigned scan) {
    constexpr std::array<unsigned,16> scans{0x02,0x03,0x04,0x05,0x10,0x11,0x12,0x13,0x1E,0x1F,0x20,0x21,0x2C,0x2D,0x2E,0x2F};
    constexpr std::array<int,16> keys{1,2,3,12,4,5,6,13,7,8,9,14,10,0,11,15};
    if(pongControls) { if(scan==0x11)return 1; if(scan==0x1F)return 4; if(scan==0xC8)return 12; if(scan==0xD0)return 13; }
    for(std::size_t n=0;n<scans.size();++n)if(scan==scans[n])return keys[n];
    return -1;
}
unsigned scanCode(LPARAM data) { return static_cast<unsigned>((data>>16)&255)|((data&(1L<<24))?128u:0u); }
void updateKeys() {
    std::array<bool,16> keys{};
    if(active&&!modal) {
        for(unsigned scan=0;scan<physicalKeys.size();++scan)if(physicalKeys[scan]) {
            const int key=scanToKey(scan); if(key>=0)keys[key]=true;
        }
        pads.add(keys);
    }
    for(std::uint8_t key=0;key<16;++key)cpu.setKey(key,keys[key]);
}
void command(UINT id) {
    if(id>=Profile0&&id<Profile0+4) { changeConfig(chip8::Config::preset(static_cast<chip8::Profile>(id-Profile0))); return; }
    if(id>=Speed0&&id<Speed0+Speeds.size()) { scheduler.setHz(Speeds[id-Speed0]); clockReset=true; persistSettings(); refresh(); return; }
    if(id>=Palette0&&id<Palette0+4) { palette=static_cast<int>(id-Palette0); persistSettings(); redraw(); return; }
    if(id>=Quirk0&&id<Quirk0+7) { const auto bits=static_cast<std::uint8_t>(cpu.config().quirks()^(1u<<(id-Quirk0))); changeConfig(chip8::Config::fromQuirks(cpu.config().profile,bits)); return; }
    switch(id) {
    case Open: openFile(); break;
    case Demo: commitGame(chip8_demo::Demo,L"Демонстрация C8",chip8::Config::preset(chip8::Profile::Modern),1500); break;
    case Pong2: commitGame(chip8_demo::Pong2,L"Pong — два игрока",chip8::Config::preset(chip8::Profile::Modern),6000); break;
    case PongClassic: commitGame(chip8_demo::PongClassic,L"Pong — исходный ROM",chip8::Config::preset(chip8::Profile::Modern),700); break;
    case Breakout: commitGame(chip8_demo::Breakout,L"Breakout",chip8::Config::preset(chip8::Profile::Modern),700); break;
    case Restart: restart(); break;
    case Pause: paused=!paused; clockReset=true; audio.silence(); refresh(); break;
    case Save: saveSlot(); break;
    case Load: loadSlot(); break;
    case Screenshot: screenshot(); break;
    case Rewind: rewind(); break;
    case Step: paused=true; cpu.step(); clockReset=true; audio.silence(); refresh(); break;
    case FrameStep: paused=true; scheduler.reset(); scheduler.advance(cpu,1.0/60.0); clockReset=true; audio.silence(); refresh(); break;
    case Debug: debug=!debug; refresh(); break;
    case Fullscreen: toggleFullscreen(); break;
    case Mute: muted=!muted; audio.silence(); persistSettings(); refresh(); break;
    case Stable: stable=true; presenter.reset(); persistSettings(); refresh(); break;
    case Exact: stable=false; presenter.reset(); persistSettings(); refresh(); break;
    case AutoPause: autoPause=!autoPause; clockReset=true; persistSettings(); refresh(); break;
    case Gamepad: gamepad=!gamepad; updateKeys(); persistSettings(); refresh(); break;
    case Folder: ShellExecuteW(window,L"open",dataDir.c_str(),nullptr,nullptr,SW_SHOWNORMAL); break;
    case Exit: PostMessageW(window,WM_CLOSE,0,0); break;
    case Help:
        modal=true; audio.silence(); physicalKeys.fill(false); cpu.releaseKeys();
        MessageBoxW(window,L"Pong на двух игроков: W/S — левая ракетка; стрелки ↑/↓ — правая.\n"
            L"Стандартная CHIP-8 клавиатура: 1234 / QWER / ASDF / ZXCV.\nКлавиши работают при русской раскладке.\n\n"
            L"P — пауза, F1 — начать текущий ROM заново.\nF5/F8 — сохранить/загрузить; F6 — PNG.\n"
            L"F9 — кадр; F10 — инструкция; F12 — отладка.\nF11 — полный экран; Esc — выйти из полного экрана.\nBackspace — назад примерно на секунду; M — звук.\n\n"
            L"Старые игры: профиль CHIP-8 (VIP). Современные: Modern.\n"
            L"Super-CHIP: 128×64, прокрутка и RPL. XO-CHIP: два цветовых слоя, 64 КБ, узорный звук.\n"
            L"Профиль и особенности инструкций перезапускают ROM.\nРасширение .sc8/.xo8 выбирает профиль; для .ch8 выбирай вручную при необходимости.\n\n"
            L"Стабилизация XOR удерживает стёртый спрайт до его перерисовки; непарное стирание — до 66,7 мс. Для проверки совместимости выбирай точный экран.",
            L"Управление и совместимость",MB_OK|MB_ICONINFORMATION);
        modal=false; clockReset=true; break;
    case About:
        modal=true; audio.silence(); physicalKeys.fill(false); cpu.releaseKeys();
        MessageBoxW(window,L"CHIP-8 Studio 2.1\n\nC++20 / Win32. Собственное переносимое ядро.\n"
            L"CHIP-8, Modern Super-CHIP, XO-CHIP 1.1.\nCPU и таймеры 60 Гц синхронизированы.\n"
            L"Пиксельный экран, сохранения, отладка и перемотка.\n\n"
            L"Это интерпретатор инструкций, а не тактовая эмуляция COSMAC VIP/HP48.\n"
            L"Исторический Super-CHIP Legacy не реализован.\n\nИсходники: MIT. Отчёт о проверках находится в архиве проекта.",L"О программе",MB_OK|MB_ICONINFORMATION);
        modal=false; clockReset=true; break;
    }
    updateMenu(); redraw();
}

LRESULT CALLBACK wndProc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
    switch(message) {
    case WM_CREATE: window=hwnd; menu=makeMenu(); SetMenu(hwnd,menu); createFonts(); return 0;
    case WM_SIZE: minimized=wp==SIZE_MINIMIZED; clockReset=true; if(minimized)audio.silence(); InvalidateRect(hwnd,nullptr,FALSE); return 0;
    case WM_DPICHANGED:
        dpi=HIWORD(wp); createFonts();
        if(!fullscreen) { const auto* r=reinterpret_cast<RECT*>(lp);SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE); }
        return 0;
    case WM_GETMINMAXINFO: {
        auto* info=reinterpret_cast<MINMAXINFO*>(lp);
        if(!fullscreen) { info->ptMinTrackSize.x=ui(740);info->ptMinTrackSize.y=ui(510); }
        return 0;
    }
    case WM_ACTIVATE:
        active=LOWORD(wp)!=WA_INACTIVE; clockReset=true;
        if(!active) { physicalKeys.fill(false);cpu.releaseKeys();audio.silence(); }
        return 0;
    case WM_KILLFOCUS: physicalKeys.fill(false);cpu.releaseKeys(); return 0;
    case WM_ENTERMENULOOP: modal=true;physicalKeys.fill(false);cpu.releaseKeys();audio.silence(); return 0;
    case WM_EXITMENULOOP: modal=false;clockReset=true; return 0;
    case WM_KEYDOWN: {
        const bool repeat=(lp&(1L<<30))!=0;
        const unsigned scan=scanCode(lp);
        if((GetKeyState(VK_CONTROL)&0x8000)&&wp=='O') { if(!repeat)command(Open);return 0; }
        UINT action=0;
        switch(wp) {
        case VK_F1:action=Restart;break; case VK_F5:action=Save;break; case VK_F6:action=Screenshot;break;
        case VK_F8:action=Load;break;case VK_F9:action=FrameStep;break;case VK_F10:action=Step;break;
        case VK_F11:action=Fullscreen;break;case VK_F12:action=Debug;break;case VK_BACK:action=Rewind;break;
        case VK_ESCAPE:if(fullscreen&&!repeat)toggleFullscreen();return 0;
        }
        if(scan==0x19)action=Pause; // physical P, independent of keyboard layout
        if(scan==0x32)action=Mute;
        if(action) { if(!repeat)command(action);return 0; }
        if(scan<physicalKeys.size())physicalKeys[scan]=true;
        updateKeys(); return 0;
    }
    case WM_KEYUP: { const unsigned scan=scanCode(lp);if(scan<physicalKeys.size())physicalKeys[scan]=false;updateKeys();return 0; }
    case WM_LBUTTONUP:
        if(!fullscreen) { POINT point{static_cast<short>(LOWORD(lp)),static_cast<short>(HIWORD(lp))};for(int n=0;n<6;++n) {const auto rect=buttonRect(n);if(PtInRect(&rect,point))command(ToolbarActions[n]);} }
        return 0;
    case WM_COMMAND: command(LOWORD(wp));return 0;
    case WM_DROPFILES: {
        auto drop=reinterpret_cast<HDROP>(wp);const auto length=DragQueryFileW(drop,0,nullptr,0);
        std::wstring file(static_cast<std::size_t>(length)+1,L'\0');DragQueryFileW(drop,0,file.data(),length+1);DragFinish(drop);
        if(length)loadPath(fs::path(file.c_str()));
        return 0;
    }
    case WM_ERASEBKGND:return 1;
    case WM_PAINT: { PAINTSTRUCT ps{};const HDC dc=BeginPaint(hwnd,&ps);paint(dc);EndPaint(hwnd,&ps);return 0; }
    case WM_CLOSE: persistSettings();saveFlags();DestroyWindow(hwnd);return 0;
    case WM_DESTROY:
        closing=true;audio.stop();physicalKeys.fill(false);cpu.releaseKeys();back.clear();PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(hwnd,message,wp,lp);
}

bool createWindow(HINSTANCE instance,int show) {
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.hInstance=instance;wc.lpfnWndProc=wndProc;
    wc.lpszClassName=ClassName;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(IDI_APP_ICON));wc.hIconSm=wc.hIcon;
    if(!RegisterClassExW(&wc))return false;
    RECT rect{0,0,1050,710};AdjustWindowRectEx(&rect,WS_OVERLAPPEDWINDOW,TRUE,0);
    if(!CreateWindowExW(WS_EX_ACCEPTFILES,ClassName,L"CHIP-8 Studio",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,
        rect.right-rect.left,rect.bottom-rect.top,nullptr,nullptr,instance,nullptr))return false;
    using GetDpi=UINT(WINAPI*)(HWND);
    const auto getDpi=procedure<GetDpi>(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow");
    if(getDpi) { dpi=getDpi(window);createFonts(); }
    BOOL dark=TRUE; DwmSetWindowAttribute(window,20,&dark,sizeof(dark));
    ShowWindow(window,show);UpdateWindow(window);return true;
}

bool captureWindow(const fs::path& path) {
    RECT bounds{};GetWindowRect(window,&bounds);
    HDC source=GetWindowDC(window),target=CreateCompatibleDC(source);
    if(!source||!target) { if(source)ReleaseDC(window,source);if(target)DeleteDC(target);return false; }
    HBITMAP bitmap=CreateCompatibleBitmap(source,bounds.right-bounds.left,bounds.bottom-bounds.top);
    if(!bitmap) { DeleteDC(target);ReleaseDC(window,source);return false; }
    const auto old=SelectObject(target,bitmap);
    BitBlt(target,0,0,bounds.right-bounds.left,bounds.bottom-bounds.top,source,0,0,SRCCOPY);
    SelectObject(target,old);
    CLSID encoder{};bool ok=false;
    if(pngEncoder(encoder)) { Gdiplus::Bitmap image(bitmap,nullptr);ok=image.Save(path.c_str(),&encoder,nullptr)==Gdiplus::Ok; }
    DeleteObject(bitmap);DeleteDC(target);ReleaseDC(window,source);return ok;
}

int smokeTest(const fs::path& directory) {
    // Runs through the real Windows entry point, HWND, GDI, input messages,
    // save/load, fullscreen and PNG APIs. It does not validate physical audio.
    std::error_code ec;fs::create_directories(directory,ec);
    dataDir=directory;settings=dataDir/L"smoke-settings.ini";
    bool ok=true;
    auto require=[&](bool condition){ok=condition&&ok;};
    for(const auto action:{Demo,Pong2,PongClassic,Breakout}) {
        command(action);
        for(int frame=0;frame<120;++frame)scheduler.advance(cpu,1.0/60.0,onFrame,onDisplay);
        require(!cpu.halted());
    }
    command(Pong2);command(Exact);command(Debug);
    for(int frame=0;frame<60;++frame)scheduler.advance(cpu,1.0/60.0,onFrame,onDisplay);
    paused=true;refresh();
    const auto initial=cpu.pc();
    require(writeAtomic(statePath(),cpu.saveState()));
    cpu.step();require(loadSlot());require(cpu.pc()==initial);
    SendMessageW(window,WM_KEYDOWN,'W',static_cast<LPARAM>(0x11u<<16));
    require(cpu.isKeyPressed(1));
    SendMessageW(window,WM_KEYUP,'W',static_cast<LPARAM>((0x11u<<16)|(1u<<30)|(1u<<31)));
    require(!cpu.isKeyPressed(1));
    SendMessageW(window,WM_KEYDOWN,VK_UP,static_cast<LPARAM>((0x48u<<16)|(1u<<24)));
    require(cpu.isKeyPressed(12));
    SendMessageW(window,WM_KILLFOCUS,0,0);require(!cpu.isKeyPressed(12));
    toggleFullscreen();require(fullscreen);toggleFullscreen();require(!fullscreen);
    SendMessageW(window,WM_SIZE,SIZE_RESTORED,0);refresh();
    require(exportScreenshot(directory/L"game.png"));
    require(captureWindow(directory/L"window.png"));
    std::ofstream report(directory/L"smoke-result.txt");report<<(ok?"PASS":"FAIL")<<"\nNative HWND/GDI/input/save-state/fullscreen/PNG paths exercised.\n";
    DestroyWindow(window);
    return ok?0:2;
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    try {
        using SetDpi=BOOL(WINAPI*)(HANDLE);
        const auto setDpi=procedure<SetDpi>(GetModuleHandleW(L"user32.dll"),"SetProcessDpiAwarenessContext");
        if(!setDpi||!setDpi(reinterpret_cast<HANDLE>(-4)))SetProcessDPIAware();
        Gdiplus::GdiplusStartupInput graphics;
        if(Gdiplus::GdiplusStartup(&gdiplusToken,&graphics,nullptr)!=Gdiplus::Ok)return 1;
        exeDir=executableDirectory();initializeData();
        if(!createWindow(instance,show)) { error(L"Не удалось создать окно.");Gdiplus::GdiplusShutdown(gdiplusToken);return 1; }
        int argc=0;LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);
        if(argv&&argc>=3&&wcscmp(argv[1],L"--smoke-test")==0) {
            const auto directory=fs::path(argv[2]);LocalFree(argv);
            const int result=smokeTest(directory);
            for(auto f:{font,titleFont,monoFont})if(f)DeleteObject(f);
            Gdiplus::GdiplusShutdown(gdiplusToken);return result;
        }
        commitGame(chip8_demo::Pong2,L"Pong — два игрока",chip8::Config::preset(chip8::Profile::Modern),6000,{},false);
        if(argv&&argc>1)loadPath(fs::path(argv[1]));
        if(argv)LocalFree(argv);
        if(!audio.start())toast(L"Звуковое устройство недоступно. Эмуляция продолжает работать.");
        pads.init();timeBeginPeriod(1);
        HANDLE timer=CreateWaitableTimerExW(nullptr,nullptr,2,TIMER_ALL_ACCESS);
        if(!timer)timer=CreateWaitableTimerW(nullptr,FALSE,nullptr);
        auto previous=Clock::now(),lastStatus=previous,lastPad=previous;
        while(!closing) {
            MSG message{};
            for(int n=0;n<64&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE);++n) {
                if(message.message==WM_QUIT) { closing=true;break; }
                TranslateMessage(&message);DispatchMessageW(&message);
            }
            if(closing)break;
            const auto now=Clock::now();
            double elapsed=std::chrono::duration<double>(now-previous).count();previous=now;
            if(clockReset) { elapsed=0;scheduler.reset();clockReset=false; }
            if(now-lastPad>=std::chrono::milliseconds(4)) {updateKeys();lastPad=now;}
            const bool wasHalted=cpu.halted();
            if(running())scheduler.advance(cpu,elapsed,onFrame,onDisplay);
            if(cpu.halted()&&!wasHalted) {audio.silence();refresh();}
            audio.pump(running()&&!muted);
            if(now-lastStatus>=std::chrono::milliseconds(250)) {redraw();lastStatus=now;}
            // Batch inexpensive CPU cycles; wake for input/audio or the next
            // display deadline, rather than once per emulated instruction.
            const double wait=running()?std::clamp(scheduler.untilNextFrame(),0.0002,0.002):0.004;
            if(timer) {
                LARGE_INTEGER due{};due.QuadPart=-std::max<LONGLONG>(1,static_cast<LONGLONG>(wait*10000000.0));
                if(SetWaitableTimer(timer,&due,0,nullptr,nullptr,FALSE))MsgWaitForMultipleObjectsEx(1,&timer,20,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
                else MsgWaitForMultipleObjectsEx(0,nullptr,1,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            } else MsgWaitForMultipleObjectsEx(0,nullptr,1,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
        }
        if(timer)CloseHandle(timer);
        timeEndPeriod(1);audio.stop();
        for(auto f:{font,titleFont,monoFont})if(f)DeleteObject(f);
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return 0;
    } catch(const std::exception& problem) {
        MessageBoxW(window,wide(problem.what()).c_str(),L"CHIP-8 Studio — ошибка",MB_OK|MB_ICONERROR);
        audio.stop();if(gdiplusToken)Gdiplus::GdiplusShutdown(gdiplusToken);return 1;
    }
}
