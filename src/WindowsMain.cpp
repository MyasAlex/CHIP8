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
#include <bcrypt.h>
#include <commctrl.h>

#include "Chip8Core.hpp"
#include "DemoRom.hpp"
#include "CatalogData.hpp"
#include "StudioMedia.hpp"
#include "StudioStorage.hpp"
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
#include <bit>
#include <memory>
#include <map>
#include <set>
#include <cwctype>
#include <optional>

#ifdef _MSC_VER
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "Gdiplus.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Bcrypt.lib")
#pragma comment(lib, "Comctl32.lib")
#endif

namespace {
using Clock = std::chrono::steady_clock;
namespace fs = std::filesystem;
enum : UINT {
    Open = 1001, Demo, Restart, Folder, Exit, Pong2, PongClassic, Breakout,
    Pause, Save, Load, Screenshot, Rewind, Step, FrameStep, Debug, Fullscreen, Mute,
    About, Help, Profile0 = 1100, Speed0 = 1200, Palette0 = 1300, Quirk0 = 1400,
    Stable = 1500, Exact, AutoPause, Gamepad, Smooth, Crt, CrtLess, CrtMore,
    Library, Favorite, Favorites, Recent, Search, ImportFolder, Remap, Keypad,
    Autosave, ResumeAuto, Gif, Mp4, StopMedia, RecordInput, PlayInput, StopInput,
    Slow, Turbo, Metrics, Compare, Breakpoint, ClearBreakpoints, Memory, Report,
    ExportSettings, ImportSettings, Portable, Achievements, RomPalette,
    Slot0=1600, Rewind30=1620, Rewind60, Rewind120, LibraryAll=1630
};
constexpr std::array<int, 7> Speeds{420,700,1000,1500,3000,6000,12000};
constexpr wchar_t ClassName[] = L"CHIP8StudioWindowV3";
struct Palette { const wchar_t* name; std::array<std::uint32_t,4> color; };
constexpr std::array<Palette,8> Palettes{{
    {L"Лёд", {0x0B1420,0xD8F3FF,0x66B3FF,0xFFBD86}},
    {L"Зелёный", {0x07150E,0x98F5AD,0x49B889,0xFCED96}},
    {L"Янтарный", {0x1B1206,0xFFD080,0xF0784B,0xFFFFCA}},
    {L"Классический", {0x050505,0xF0F0F0,0x777777,0xFFFFFF}},
    {L"Лиловый", {0x16112C,0xF1CCFF,0xAA8AE8,0x73F2DD}},
    {L"Game Boy", {0x0F380F,0x9BBC0F,0x306230,0x8BAC0F}},
    {L"Закат", {0x1C1024,0xFFCB7D,0xEF6F88,0xB4A2FF}},
    {L"Океан", {0x061B25,0x7EEDE0,0x2B9BAE,0xE8E2A0}}
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
bool smooth=false,crt=false,showLibrary=false,showKeypad=false,showMetrics=true;
bool autoSave=true,resumeAuto=true,romPalette=true,portable=false,rewinding=false,rewindWasPaused=false;
bool slowMode=false,turboMode=false,timelineDragging=false,traceEnabled=false;
bool customControls=false;
bool smokeMode=false;
std::vector<std::wstring> smokeErrors;
int intensity=45,slot=0,rewindSeconds=120,virtualHeld=-1,remapIndex=-1;
std::array<unsigned,16> bindings{};
std::array<unsigned,16> pendingBindings{};
bool remapWasPaused=false;
std::array<int,6> gameControls{7,9,5,8,6,4};
std::array<std::uint32_t,4> gameColors{};
std::array<std::uint32_t,chip8::Chip8::DisplaySize> phosphor{};
chip8::Chip8::Display rewindDisplay{};
bool rewindPreview=false;
int rewindW=64,rewindH=32;
std::size_t rewindCursor=0;
std::uint64_t vmFrames=0,playFrames=0,autosaveFrame=0;
chip8::RewindBuffer history;
enum class InputMode { None,Record,Play };
InputMode inputMode=InputMode::None;
chip8::Replay inputReplay;
std::size_t replayCursor=0;
chip8_media::GifWriter gif;
chip8_media::Mp4Writer mp4;
fs::path mediaPath;
std::array<std::int16_t,1600> mediaSound{};
double mediaPhase=0,mediaGain=0,hostAccumulator=0;
std::set<std::uint16_t> breakpoints;
bool skipBreakpoint=false;
struct TraceEntry {std::wstring decoded;std::array<std::uint8_t,16> registers;};
std::deque<TraceEntry> trace;
std::array<std::uint8_t,16> previousRegisters{};
std::uint16_t memoryAddress=0x200;
double measuredFps=0,measuredIps=0,measuredCpu=0;
std::uint64_t metricFrames=0,metricInstructions=0;
HWND searchEdit=nullptr;
std::wstring libraryQuery;
int libraryFilter=0,libraryPage=0;
struct LibraryGame {
    std::wstring title,authors,description,platform,sha;
    fs::path path,cover;
    int builtin=-1;const chip8_catalog::Program* metadata=nullptr;
    bool favorite=false;std::uint64_t seconds=0,last=0;
};
std::vector<LibraryGame> library;
std::vector<std::size_t> visibleGames;
std::vector<std::pair<RECT,std::size_t>> libraryTiles;
std::map<std::wstring,std::unique_ptr<Gdiplus::Bitmap>> covers;
std::wstring currentSha;
const chip8_catalog::Program* currentMetadata=nullptr;
int palette = 0;
std::wstring romName, notice;
Clock::time_point noticeUntil{};
fs::path exeDir, dataDir, settings, romPath;
std::vector<std::uint8_t> romBytes;
std::array<bool,256> physicalKeys{};
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
void releaseInput();
void error(const std::wstring& text) {
    if(smokeMode){smokeErrors.push_back(text);toast(text);return;}
    modal=true;releaseInput();
    MessageBoxW(window,text.c_str(),L"CHIP-8 Studio",MB_OK|MB_ICONERROR);
    modal=false; clockReset=true;
}
bool running() { return !paused && !showLibrary && !rewinding && !minimized && !modal && (!autoPause || active) && !cpu.halted(); }
std::wstring gameId() { return hex(cpu.romHash(),16); }
fs::path statePath() { return dataDir / (gameId()+L".slot"+std::to_wstring(slot)+L".c8state"); }
fs::path autoPath() {return dataDir/(gameId()+L".auto.c8state");}
void saveAutomatic();void stopInput(bool save=true);void stopMedia();void rebuildLibrary();
void showCatalog(bool value);void remapKeys();void updateKeys();
bool exportScreenshot(const fs::path& path);
std::string utf8(const std::wstring& value) {
    if(value.empty())return {};
    const int size=WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);
    std::string out(static_cast<std::size_t>(size),0);WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),out.data(),size,nullptr,nullptr);return out;
}
std::wstring readSetting(const wchar_t* section,const wchar_t* key,const wchar_t* fallback=L"") {
    std::wstring value(32768,0);const auto size=GetPrivateProfileStringW(section,key,fallback,value.data(),static_cast<DWORD>(value.size()),settings.c_str());value.resize(size);return value;
}
std::uint64_t numberSetting(const wchar_t* section,const wchar_t* key) {
    const auto value=readSetting(section,key);try{return std::stoull(value);}catch(...){return 0;}
}
std::wstring sha1(const std::vector<std::uint8_t>& bytes) {
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;DWORD objectSize=0,returned=0;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA1_ALGORITHM,nullptr,0)<0)return {};
    std::array<unsigned char,20> digest{};
    bool ok=BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&objectSize),sizeof(objectSize),&returned,0)>=0;
    std::vector<unsigned char> object(objectSize);
    if(ok)ok=BCryptCreateHash(algorithm,&hash,object.data(),objectSize,nullptr,0,0)>=0;
    if(ok)ok=BCryptHashData(hash,const_cast<PUCHAR>(bytes.data()),static_cast<ULONG>(bytes.size()),0)>=0;
    if(ok)ok=BCryptFinishHash(hash,digest.data(),static_cast<ULONG>(digest.size()),0)>=0;
    if(hash)BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm,0);
    if(!ok)return {};
    std::wstring result;for(auto byte:digest)result+=hex(byte,2);
    std::transform(result.begin(),result.end(),result.begin(),[](wchar_t c){return static_cast<wchar_t>(std::towlower(c));});return result;
}

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
                keys[gameControls[2]]=keys[gameControls[2]]||up;keys[gameControls[3]]=keys[gameControls[3]]||down;
                keys[gameControls[0]]=keys[gameControls[0]]||(buttons&XINPUT_GAMEPAD_DPAD_LEFT)||state.Gamepad.sThumbLX<-16000;
                keys[gameControls[1]]=keys[gameControls[1]]||(buttons&XINPUT_GAMEPAD_DPAD_RIGHT)||state.Gamepad.sThumbLX>16000;
                for(const auto& pair:{std::pair<WORD,int>{XINPUT_GAMEPAD_A,gameControls[4]},{XINPUT_GAMEPAD_B,gameControls[5]},{XINPUT_GAMEPAD_X,0},{XINPUT_GAMEPAD_Y,15},{XINPUT_GAMEPAD_START,10}})
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
    const auto testData=smokeMode?dataDir:fs::path{};
    wchar_t path[MAX_PATH]{};
    if(SUCCEEDED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,path)))
        dataDir=fs::path(path)/L"CHIP8Studio";
    else dataDir=exeDir/L"userdata";
    portable=fs::exists(exeDir/L"portable.ini");if(portable)dataDir=exeDir/L"userdata";
    if(!testData.empty())dataDir=testData;
    std::error_code ec;
    fs::create_directories(dataDir,ec);
    settings=dataDir/L"settings.ini";
    palette=std::clamp<int>(GetPrivateProfileIntW(L"App",L"Palette",0,settings.c_str()),0,7);
    muted=GetPrivateProfileIntW(L"App",L"Mute",0,settings.c_str())!=0;
    stable=GetPrivateProfileIntW(L"App",L"Stable",1,settings.c_str())!=0;
    autoPause=GetPrivateProfileIntW(L"App",L"AutoPause",1,settings.c_str())!=0;
    gamepad=GetPrivateProfileIntW(L"App",L"Gamepad",1,settings.c_str())!=0;
    smooth=GetPrivateProfileIntW(L"App",L"Smooth",0,settings.c_str())!=0;
    crt=GetPrivateProfileIntW(L"App",L"CRT",0,settings.c_str())!=0;
    intensity=std::clamp<int>(GetPrivateProfileIntW(L"App",L"CRTIntensity",45,settings.c_str()),0,100);
    autoSave=GetPrivateProfileIntW(L"App",L"Autosave",1,settings.c_str())!=0;
    resumeAuto=GetPrivateProfileIntW(L"App",L"Resume",1,settings.c_str())!=0;
    showKeypad=GetPrivateProfileIntW(L"App",L"Keypad",0,settings.c_str())!=0;
    showMetrics=GetPrivateProfileIntW(L"App",L"Metrics",1,settings.c_str())!=0;
    romPalette=GetPrivateProfileIntW(L"App",L"RomPalette",1,settings.c_str())!=0;
    rewindSeconds=GetPrivateProfileIntW(L"App",L"RewindSeconds",120,settings.c_str());
    if(rewindSeconds!=30&&rewindSeconds!=60&&rewindSeconds!=120)rewindSeconds=120;
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
    ok=writeSetting(id.c_str(),L"Slot",std::to_wstring(slot))&&ok;
    ok=writeSetting(id.c_str(),L"Palette",std::to_wstring(palette))&&ok;
    std::wstring map;for(auto scan:bindings)map+=std::to_wstring(scan)+L",";
    ok=writeSetting(id.c_str(),L"Keys",map)&&ok;
    ok=writeSetting(id.c_str(),L"CustomKeys",std::to_wstring(customControls))&&ok;
    ok=writeSetting(L"App",L"Palette",std::to_wstring(palette))&&ok;
    ok=writeSetting(L"App",L"Mute",std::to_wstring(muted))&&ok;
    ok=writeSetting(L"App",L"Stable",std::to_wstring(stable))&&ok;
    ok=writeSetting(L"App",L"AutoPause",std::to_wstring(autoPause))&&ok;
    ok=writeSetting(L"App",L"Gamepad",std::to_wstring(gamepad))&&ok;
    for(const auto& item:{std::pair<const wchar_t*,int>{L"Smooth",smooth},{L"CRT",crt},{L"CRTIntensity",intensity},
        {L"Autosave",autoSave},{L"Resume",resumeAuto},{L"Keypad",showKeypad},{L"Metrics",showMetrics},
        {L"RomPalette",romPalette},{L"RewindSeconds",rewindSeconds}})ok=writeSetting(L"App",item.first,std::to_wstring(item.second))&&ok;
    for(auto& game:library)if(game.sha==currentSha) {
        const auto section=L"Library."+game.sha;
        game.seconds+=playFrames/60;playFrames%=60;
        writeSetting(section.c_str(),L"Seconds",std::to_wstring(game.seconds));
        writeSetting(section.c_str(),L"Last",std::to_wstring(game.last));break;
    }
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
fs::path assetPath(const std::string& name) {
    if(name.empty())return {};
    const fs::path relative=wide(name);
    for(const auto& base:{exeDir,fs::current_path(),exeDir.parent_path().parent_path(),exeDir.parent_path().parent_path().parent_path()}) {
        const auto path=base/relative;std::error_code ec;if(fs::is_regular_file(path,ec))return path;
    }
    return exeDir/relative;
}
const chip8_catalog::Program* knownProgram(const std::wstring& sha) {
    const auto key=utf8(sha);
    for(const auto& p:chip8_catalog::Games)if(key==p.sha1)return &p;
    for(const auto& p:chip8_catalog::Known)if(key==p.sha1)return &p;
    return nullptr;
}
std::wstring platformLabel(const char* id) {
    const std::string name(id);
    if(name=="chip8"||name=="originalChip8"||name=="modernChip8")return L"CHIP-8";
    if(name=="schip"||name=="superchip"||name=="superchip1"||name=="superchipModern")return L"Super-CHIP";
    if(name=="xochip")return L"XO-CHIP";
    if(name=="chip48")return L"CHIP-48";
    return wide(name);
}
std::wstring localDate(std::uint64_t seconds) {
    if(!seconds||seconds>(~std::uint64_t{0}-116444736000000000ull)/10000000)return L"ещё не запускали";
    ULARGE_INTEGER number{};number.QuadPart=seconds*10000000+116444736000000000ull;FILETIME utc{number.LowPart,number.HighPart},local{};SYSTEMTIME time{};
    if(!FileTimeToLocalFileTime(&utc,&local)||!FileTimeToSystemTime(&local,&time))return L"—";
    std::wostringstream out;out<<std::setfill(L'0')<<std::setw(2)<<time.wDay<<L'.'<<std::setw(2)<<time.wMonth<<L'.'<<time.wYear<<L' '<<std::setw(2)<<time.wHour<<L':'<<std::setw(2)<<time.wMinute;return out.str();
}
void libraryStatistics(LibraryGame& game) {
    const auto section=L"Library."+game.sha;
    game.favorite=GetPrivateProfileIntW(section.c_str(),L"Favorite",0,settings.c_str())!=0;
    game.seconds=numberSetting(section.c_str(),L"Seconds");game.last=numberSetting(section.c_str(),L"Last");
}
void initializeLibrary() {
    library.clear();covers.clear();
    const std::array<const std::vector<std::uint8_t>*,4> builtin{&chip8_demo::Pong2,&chip8_demo::PongClassic,&chip8_demo::Breakout,&chip8_demo::Demo};
    const std::array<std::wstring,4> titles{L"Pong — два игрока",L"Pong — исходный ROM",L"Breakout",L"Демонстрация C8"};
    for(int n=0;n<4;++n) {
        LibraryGame game;game.title=titles[n];game.sha=sha1(*builtin[n]);game.builtin=n;
        game.platform=L"CHIP-8";game.authors=n==0||n==3?L"CHIP-8 Studio":L"Classic collection";
        game.description=n==0?L"Два игрока: W/S — слева, стрелки вверх/вниз — справа.":n==3?L"Встроенная анимация, проверка экрана.":L"Оригинальная классическая игра.";
        game.metadata=knownProgram(game.sha);libraryStatistics(game);library.push_back(std::move(game));
    }
    for(const auto& p:chip8_catalog::Games) {
        const auto sha=wide(p.sha1);
        auto existing=std::find_if(library.begin(),library.end(),[&](const auto& game){return game.sha==sha;});
        if(existing!=library.end()){existing->cover=assetPath(p.cover);continue;}
        LibraryGame game;game.sha=sha;game.title=wide(p.title);game.authors=wide(p.authors);
        game.description=wide(p.description);game.platform=platformLabel(p.platform);
        game.path=assetPath(p.path);game.cover=assetPath(p.cover);game.metadata=&p;
        libraryStatistics(game);library.push_back(std::move(game));
    }
    std::error_code ec;fs::create_directories(dataDir/L"roms",ec);
    for(const auto& entry:fs::directory_iterator(dataDir/L"roms",fs::directory_options::skip_permission_denied,ec)) {
        if(!entry.is_regular_file(ec))continue;
        std::vector<std::uint8_t> bytes;if(!readFile(entry.path(),bytes,65024)||bytes.size()<2)continue;
        const auto sha=sha1(bytes);if(sha.empty())continue;
        if(std::any_of(library.begin(),library.end(),[&](const auto& game){return game.sha==sha;}))continue;
        LibraryGame game;game.sha=sha;game.path=entry.path();game.metadata=knownProgram(sha);
        const auto section=L"Library."+sha;
        game.title=readSetting(section.c_str(),L"Title",entry.path().stem().c_str());
        game.platform=game.metadata?platformLabel(game.metadata->platform):L"Импорт";
        if(game.metadata){game.authors=wide(game.metadata->authors);game.description=wide(game.metadata->description);}
        libraryStatistics(game);library.push_back(std::move(game));
    }
    rebuildLibrary();
}
std::wstring lowercase(std::wstring value) {
    std::transform(value.begin(),value.end(),value.begin(),[](wchar_t c){return static_cast<wchar_t>(std::towlower(c));});return value;
}
void rebuildLibrary() {
    visibleGames.clear();const auto query=lowercase(libraryQuery);
    for(std::size_t index=0;index<library.size();++index) {
        const auto& game=library[index];
        if(libraryFilter==1&&!game.favorite)continue;
        if(libraryFilter==2&&!game.last)continue;
        if(!query.empty()&&lowercase(game.title+L" "+game.authors+L" "+game.platform).find(query)==std::wstring::npos)continue;
        visibleGames.push_back(index);
    }
    if(libraryFilter==2)std::stable_sort(visibleGames.begin(),visibleGames.end(),[](auto a,auto b){return library[a].last>library[b].last;});
    else std::stable_sort(visibleGames.begin(),visibleGames.end(),[](auto a,auto b){
        if(library[a].builtin>=0||library[b].builtin>=0)return library[a].builtin>=0&&library[b].builtin<0;
        return lowercase(library[a].title)<lowercase(library[b].title);
    });
    libraryPage=0;
}
bool importRom(const fs::path& path) {
    std::vector<std::uint8_t> bytes;if(!readFile(path,bytes,65024)||bytes.size()<2)return false;
    const auto sha=sha1(bytes);if(sha.empty())return false;
    if(std::any_of(library.begin(),library.end(),[&](const auto& game){return game.sha==sha;}))return true;
    std::error_code ec;fs::create_directories(dataDir/L"roms",ec);
    const auto destination=dataDir/L"roms"/(sha+L".ch8");if(!writeAtomic(destination,bytes))return false;
    LibraryGame game;game.sha=sha;game.path=destination;game.title=path.stem().wstring();game.metadata=knownProgram(sha);
    if(game.metadata){game.title=wide(game.metadata->title);game.authors=wide(game.metadata->authors);game.description=wide(game.metadata->description);game.platform=platformLabel(game.metadata->platform);}
    else {
        game.platform=L"Импорт";chip8::Chip8 fingerprint(chip8::Config::preset(chip8::Profile::XoChip));fingerprint.loadRom(bytes);
        const auto id=hex(fingerprint.romHash(),16);
        if(readSetting(id.c_str(),L"Profile").empty()){
            const auto ext=lowercase(path.extension().wstring());const auto profile=ext==L".xo8"||bytes.size()>3584?chip8::Profile::XoChip:ext==L".sc8"?chip8::Profile::SuperChip:chip8::Profile::Modern;
            const auto config=chip8::Config::preset(profile);writeSetting(id.c_str(),L"Profile",std::to_wstring(static_cast<int>(profile)));writeSetting(id.c_str(),L"Quirks",std::to_wstring(config.quirks()));
            writeSetting(id.c_str(),L"Hz",std::to_wstring(profile==chip8::Profile::XoChip?6000:profile==chip8::Profile::SuperChip?1500:700));
        }
    }
    writeSetting((L"Library."+sha).c_str(),L"Title",game.title);libraryStatistics(game);library.push_back(std::move(game));rebuildLibrary();return true;
}
void saveAutomatic() {
    if(autoSave&&!romBytes.empty()&&inputMode==InputMode::None&&!cpu.halted()) {
        if(!writeAtomic(autoPath(),cpu.saveState()))toast(L"Автосохранение не записано: проверь папку данных.");
        autosaveFrame=vmFrames;
    }
}
void defaultBindings() {
    constexpr std::array<unsigned,16> scans{0x2D,0x02,0x03,0x04,0x10,0x11,0x12,0x1E,0x1F,0x20,0x2C,0x2E,0x05,0x13,0x21,0x2F};
    bindings=scans;
}
std::vector<std::uint8_t> timelineState() {
    const auto state=cpu.saveState();std::vector<std::uint8_t> out;
    auto put=[&](std::uint64_t value,unsigned n){for(unsigned i=0;i<n;++i)out.push_back(static_cast<std::uint8_t>(value>>(i*8)));};
    put(state.size(),4);const auto phase=scheduler.phase();put(std::bit_cast<std::uint64_t>(phase.hz),8);
    put(std::bit_cast<std::uint64_t>(phase.cpu),8);put(std::bit_cast<std::uint64_t>(phase.frame),8);
    put(vmFrames,8);put(presenter.width(),2);put(presenter.height(),2);
    out.insert(out.end(),state.begin(),state.end());out.insert(out.end(),presenter.display().begin(),presenter.display().end());return out;
}
bool restoreTimeline(std::size_t index) {
    std::vector<std::uint8_t> bytes;if(!history.restore(index,bytes)||bytes.size()<40+chip8::Chip8::DisplaySize)return false;
    std::size_t position=0;auto get=[&](unsigned n){std::uint64_t v=0;for(unsigned i=0;i<n;++i)v|=static_cast<std::uint64_t>(bytes[position++])<<(i*8);return v;};
    const auto size=get(4);chip8::Scheduler::Phase phase{std::bit_cast<double>(get(8)),std::bit_cast<double>(get(8)),std::bit_cast<double>(get(8))};
    const auto frame=get(8);const auto width=get(2),height=get(2);
    if(position+size+chip8::Chip8::DisplaySize!=bytes.size()||(width!=64&&width!=128)||(height!=32&&height!=64))return false;
    const std::vector<std::uint8_t> state(bytes.begin()+position,bytes.begin()+position+size);std::string problem;
    if(!cpu.loadState(state,problem)||!scheduler.restorePhase(phase))return false;
    position+=size;std::copy_n(bytes.begin()+position,rewindDisplay.size(),rewindDisplay.begin());
    rewindW=static_cast<int>(width);rewindH=static_cast<int>(height);rewindPreview=true;
    vmFrames=frame;autosaveFrame=frame;rewindCursor=index;audio.silence();redraw();return true;
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
    check(Stable,stable&&!smooth);check(Smooth,smooth);check(Exact,!stable&&!smooth);check(Crt,crt);
    check(AutoPause,autoPause);check(Gamepad,gamepad);check(Library,showLibrary);check(Keypad,showKeypad);
    check(Autosave,autoSave);check(ResumeAuto,resumeAuto);check(RomPalette,romPalette);check(Metrics,showMetrics);
    check(Slow,slowMode);check(Turbo,turboMode);check(Portable,portable);
    check(RecordInput,inputMode==InputMode::Record);check(PlayInput,inputMode==InputMode::Play);
    check(Gif,gif.active());check(Mp4,mp4.active());
    for(unsigned n=0;n<4;++n)check(Profile0+n,static_cast<unsigned>(cpu.config().profile)==n);
    for(unsigned n=0;n<Palettes.size();++n)check(Palette0+n,palette==static_cast<int>(n));
    for(unsigned n=0;n<10;++n)check(Slot0+n,slot==static_cast<int>(n));
    check(Rewind30,rewindSeconds==30);check(Rewind60,rewindSeconds==60);check(Rewind120,rewindSeconds==120);
    for(unsigned n=0;n<Speeds.size();++n)check(Speed0+n,std::abs(scheduler.hz()-Speeds[n])<0.5);
    for(unsigned n=0;n<7;++n)check(Quirk0+n,(cpu.config().quirks()&(1u<<n))!=0);
    SetWindowTextW(window,(L"CHIP-8 Studio — "+romName+(paused?L" [ПАУЗА]":L"")).c_str());
}
void resetHistory() {history=chip8::RewindBuffer(64u*1024u*1024u,static_cast<std::uint64_t>(rewindSeconds)*60);rewindPreview=false;rewinding=false;vmFrames=autosaveFrame=0;history.capture(timelineState(),0);}
void refresh(bool force = true) {
    presenter.publish(cpu,stable||smooth,force);
    if(force){const auto& colors=romPalette&&currentMetadata?gameColors:Palettes[palette].color;for(std::size_t n=0;n<phosphor.size();++n)phosphor[n]=colors[presenter.display()[n]&3];}
    updateMenu();redraw();
}
bool commitGame(const std::vector<std::uint8_t>& bytes,const std::wstring& name,
                chip8::Config config,int defaultHz,const fs::path& path={},bool remember=true,bool useSaved=true) {
    chip8::Chip8 next(chip8::Config::preset(chip8::Profile::XoChip));
    if(!next.loadRom(bytes)) { error(L"ROM должен содержать от 2 до 65024 байт."); return false; }
    const auto id=hex(next.romHash(),16);
    int hz=defaultHz;
    if(useSaved) {
        const int p=std::clamp<int>(GetPrivateProfileIntW(id.c_str(),L"Profile",static_cast<int>(config.profile),settings.c_str()),0,3);
        if(p!=static_cast<int>(config.profile))config=chip8::Config::preset(static_cast<chip8::Profile>(p));
        const int q=GetPrivateProfileIntW(id.c_str(),L"Quirks",config.quirks(),settings.c_str());
        if(q>=0&&q<=127)config=chip8::Config::fromQuirks(config.profile,static_cast<std::uint8_t>(q));
        hz=std::clamp<int>(GetPrivateProfileIntW(id.c_str(),L"Hz",hz,settings.c_str()),60,100000);
    }
    next.setConfig(config);
    if(!next.loadRom(bytes)) { error(L"Этот ROM слишком велик для выбранного профиля. Выбери XO-CHIP."); return false; }
    stopInput();stopMedia();
    if(remember) {saveAutomatic();persistSettings();saveFlags();}
    restoreFlags(next);
    next.seedRandom(static_cast<std::uint32_t>(Clock::now().time_since_epoch().count()));
    cpu=std::move(next); romBytes=bytes; romName=name; romPath=path;
    pongControls=(bytes==chip8_demo::Pong2);
    currentSha=sha1(bytes);currentMetadata=knownProgram(currentSha);playFrames=0;
    gameControls=currentMetadata?currentMetadata->controls:std::array<int,6>{7,9,5,8,6,4};
    for(auto& key:gameControls)key=std::clamp(key,0,15);
    gameColors=currentMetadata?currentMetadata->colors:Palettes[palette].color;
    defaultBindings();
    const auto map=readSetting(id.c_str(),L"Keys");std::wistringstream mapping(map);bool valid=true;
    std::array<unsigned,16> custom{};
    for(auto& scan:custom){wchar_t comma=0;if(!(mapping>>scan>>comma)||comma!=L','||scan>255){valid=false;break;}}
    if(valid)bindings=custom;
    customControls=valid&&GetPrivateProfileIntW(id.c_str(),L"CustomKeys",0,settings.c_str())!=0;
    slot=std::clamp<int>(GetPrivateProfileIntW(id.c_str(),L"Slot",0,settings.c_str()),0,9);
    palette=std::clamp<int>(GetPrivateProfileIntW(id.c_str(),L"Palette",palette,settings.c_str()),0,7);
    scheduler.setHz(hz);scheduler.reset();presenter.reset();phosphor.fill(0);trace.clear();previousRegisters=cpu.registers();
    if(useSaved&&resumeAuto) {
        std::vector<std::uint8_t> saved;std::string problem;
        if(readFile(autoPath(),saved,80000)&&cpu.loadState(saved,problem)){scheduler.setHz(hz);scheduler.reset();}
    }
    physicalKeys.fill(false);virtualHeld=-1;paused=false;clockReset=true;audio.silence();notice.clear();showCatalog(false);
    const auto last=static_cast<std::uint64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    writeSetting(L"App",L"LastGame",currentSha);
    for(auto& game:library)if(game.sha==currentSha){game.last=last;writeSetting((L"Library."+game.sha).c_str(),L"Last",std::to_wstring(last));break;}
    refresh();
    resetHistory();
    return true;
}
bool loadPath(const fs::path& path) {
    std::vector<std::uint8_t> bytes;
    if(!readFile(path,bytes,65024)||bytes.size()<2) { error(L"Не удалось прочитать ROM (2–65024 байт):\n"+path.wstring()); return false; }
    const auto ext=lowercase(path.extension().wstring());
    auto profile=bytes.size()>3584||ext==L".xo8"?chip8::Profile::XoChip:ext==L".sc8"?chip8::Profile::SuperChip:chip8::Profile::Modern;
    int hz=profile==chip8::Profile::XoChip?6000:profile==chip8::Profile::SuperChip?1500:700;
    auto config=chip8::Config::preset(profile);const auto known=knownProgram(sha1(bytes));
    if(known&&!known->runnable){error(L"ROM требует другой платформы: "+wide(known->platform)+L". Этот режим пока не реализован.");return false;}
    if(known){config=chip8::Config::fromQuirks(static_cast<chip8::Profile>(known->profile),static_cast<std::uint8_t>(known->quirks));hz=known->hz;}
    importRom(path);
    if(!commitGame(bytes,known?wide(known->title):path.filename().wstring(),config,hz,path))return false;
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
    dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_EXPLORER|OFN_ALLOWMULTISELECT;
    modal=true;releaseInput();audio.silence();
    const bool selected=GetOpenFileNameW(&dialog)!=FALSE;
    modal=false; clockReset=true;
    if(selected) {
        const fs::path first=file.c_str();const auto offset=wcslen(file.c_str())+1;
        if(file[offset]==0)loadPath(first);
        else {for(std::size_t pos=offset;pos<file.size()&&file[pos];pos+=wcslen(file.c_str()+pos)+1)importRom(first/fs::path(file.c_str()+pos));showCatalog(true);toast(L"ROM добавлены в библиотеку.");}
    }
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
    if(writeAtomic(statePath(),cpu.saveState())){exportScreenshot(statePath().wstring()+L".png");toast(L"Слот "+std::to_wstring(slot+1)+L" сохранён · F8 — загрузить");}
    else error(L"Не удалось сохранить состояние:\n"+statePath().wstring());
}
bool loadSlot() {
    stopInput();stopMedia();
    std::vector<std::uint8_t> bytes;
    if(!readFile(statePath(),bytes,80000)&&!(slot==0&&readFile(dataDir/(gameId()+L".c8state"),bytes,80000))) { error(L"Слот пуст. Нажми F5 для создания."); return false; }
    std::string problem;
    if(!cpu.loadState(bytes,problem)) { error(L"Не удалось загрузить сохранение:\n"+wide(problem)); return false; }
    cpu.releaseKeys(); physicalKeys.fill(false); scheduler.reset(); clockReset=true;
    audio.silence(); presenter.reset(); resetHistory(); persistSettings(); refresh(); toast(L"Состояние восстановлено");
    return true;
}
void rewind() {
    if(history.size()<2)return;
    if(!rewinding){stopInput();stopMedia();rewinding=true;rewindWasPaused=paused;rewindCursor=history.size()-1;physicalKeys.fill(false);cpu.releaseKeys();audio.silence();}
    if(rewindCursor)restoreTimeline(rewindCursor-1);
}
void finishRewind() {
    if(!rewinding)return;
    history.branch(rewindCursor);rewinding=false;timelineDragging=false;rewindPreview=false;
    physicalKeys.fill(false);virtualHeld=-1;cpu.releaseKeys();presenter.reset();phosphor.fill(0);
    paused=rewindWasPaused;hostAccumulator=0;clockReset=true;refresh();
}
void onFrame() {
    presenter.publish(cpu,stable||smooth);
    const auto& colors=romPalette&&currentMetadata?gameColors:Palettes[palette].color;
    for(std::size_t n=0;n<phosphor.size();++n) {
        const auto target=colors[presenter.display()[n]&3];
        if(!smooth){phosphor[n]=target;continue;}
        std::uint32_t rgb=0;for(unsigned shift:{0u,8u,16u}){
            const auto old=(phosphor[n]>>shift)&255,next=(target>>shift)&255;
            rgb|=std::max(next,static_cast<unsigned>(old*0.48))<<shift;
        }phosphor[n]=rgb;
    }
    ++metricFrames;
}

void onDisplay() {if(stable||smooth)presenter.observe(cpu);}

fs::path outputPath(const wchar_t* suffix) {
    SYSTEMTIME time{};GetLocalTime(&time);
    return dataDir/(L"CHIP8-"+gameId()+L"-"+std::to_wstring(time.wYear)+L"-"+std::to_wstring(time.wMonth)+L"-"+
        std::to_wstring(time.wDay)+L"-"+std::to_wstring(time.wHour)+L"-"+std::to_wstring(time.wMinute)+L"-"+
        std::to_wstring(time.wSecond)+L"-"+std::to_wstring(time.wMilliseconds)+suffix);
}
std::vector<std::uint32_t> screenPixels(int w,int h,bool raw=false,bool effects=true) {
    const auto& display=raw?cpu.display():rewindPreview?rewindDisplay:presenter.display();
    const int logicalW=raw?cpu.width():rewindPreview?rewindW:presenter.width();
    const int logicalH=raw?cpu.height():rewindPreview?rewindH:presenter.height();
    const auto& colors=romPalette&&currentMetadata?gameColors:Palettes[palette].color;
    std::vector<std::uint32_t> out(static_cast<std::size_t>(w)*h);
    const double strength=intensity/100.0;
    for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
        const int lx=x*logicalW/w,ly=y*logicalH/h;const auto index=static_cast<std::size_t>(ly)*128+lx;
        const auto rgb=effects&&smooth&&!raw&&!rewindPreview?phosphor[index]:colors[display[index]&3];
        std::uint32_t result=0;
        for(unsigned shift:{0u,8u,16u}) {
            double channel=(rgb>>shift)&255;
            if(effects&&crt) {
                double glow=0;
                for(const auto& d:{std::pair<int,int>{-1,0},{1,0},{0,-1},{0,1}}) {
                    const int nx=lx+d.first,ny=ly+d.second;
                    if(nx>=0&&nx<logicalW&&ny>=0&&ny<logicalH)glow+=((colors[display[static_cast<std::size_t>(ny)*128+nx]&3]>>shift)&255)/4.0;
                }
                channel+=glow*strength*0.14;
                if(y&1)channel*=1-strength*0.33;
                const double dx=2.0*x/w-1,dy=2.0*y/h-1;
                channel*=1-strength*0.16*(dx*dx+dy*dy);
            }
            result|=static_cast<std::uint32_t>(std::clamp(channel,0.0,255.0))<<shift;
        }
        out[static_cast<std::size_t>(y)*w+x]=0xFF000000u|result;
    }
    return out;
}
void stopMedia() {
    if(!gif.active()&&!mp4.active())return;
    const bool a=gif.finish(),b=mp4.finish();
    if(a&&b)toast(L"Запись сохранена: "+mediaPath.filename().wstring());
    else toast(L"Не удалось завершить запись: "+mediaPath.filename().wstring());
    updateMenu();
}
void startMedia(bool video) {
    stopMedia();mediaPath=outputPath(video?L".mp4":L".gif");
    const bool ok=video?mp4.start(mediaPath):gif.start(mediaPath);
    if(!ok){const auto reason=mp4.error();mp4.finish();std::error_code ec;fs::remove(mediaPath,ec);error(video?L"Кодек MP4 недоступен (Media Foundation). Можно записать GIF. Код: "+hex(static_cast<unsigned long>(reason),8):L"Не удалось создать GIF.");return;}
    mediaPhase=mediaGain=0;mediaSound.fill(0);
    if(video)mp4.frame(screenPixels(512,256),mediaSound);else gif.frame(screenPixels(256,128));
    toast(video?L"Запись MP4 со звуком · до 2 минут. Запись → Остановить.":L"Запись GIF · до 2 минут. Запись → Остановить.");updateMenu();
}
void recordMediaFrame() {
    if(!gif.active()&&!mp4.active())return;
    if(mp4.active()) {
        const bool on=!muted&&cpu.soundTimer()>0;const bool xo=cpu.config().xo();
        const double frequency=xo?4000*std::pow(2.0,(static_cast<int>(cpu.pitch())-64)/48.0):440;
        const std::size_t begin=vmFrames%2?0:800;
        for(std::size_t index=begin;index<begin+800;++index) {
            mediaGain+=((on?0.11:0)-mediaGain)*0.01;double value=0;
            if(on) {
                if(xo){const auto bit=static_cast<unsigned>(mediaPhase)&127;value=cpu.audioPattern()[bit/8]&(0x80>>(bit&7))?1:-1;mediaPhase=std::fmod(mediaPhase+frequency/48000,128.0);}
                else{value=std::sin(mediaPhase*6.283185307179586);mediaPhase=std::fmod(mediaPhase+frequency/48000,1.0);}
            }
            mediaSound[index]=static_cast<std::int16_t>(value*mediaGain*32767);
        }
    }
    if(vmFrames%2)return;
    bool ok=true;if(gif.active())ok=gif.frame(screenPixels(256,128));
    if(mp4.active())ok=mp4.frame(screenPixels(512,256),mediaSound)&&ok;
    if(!ok||gif.frames()>=3600||mp4.frames()>=3600)stopMedia();
}
void stopInput(bool save) {
    const auto old=inputMode;inputMode=InputMode::None;
    if(old==InputMode::Record&&save&&!inputReplay.frames.empty()) {
        const auto path=outputPath(L".c8replay");const auto bytes=inputReplay.encode();
        if(!bytes.empty()&&writeAtomic(path,bytes)){writeSetting(L"App",L"LastReplay",path.wstring());toast(L"Повтор сохранён: "+path.filename().wstring());}
        else toast(L"Не удалось сохранить повтор ввода.");
    }
    if(old!=InputMode::None){cpu.releaseKeys();physicalKeys.fill(false);clockReset=true;}
    updateMenu();
}
void startInputRecording() {
    stopInput();restart();if(cpu.halted())return;
    paused=false;showCatalog(false);scheduler.reset();cpu.releaseKeys();physicalKeys.fill(false);
    inputReplay={};inputReplay.hz=scheduler.hz();inputReplay.rom=romBytes;inputReplay.initial=cpu.saveState();
    inputMode=InputMode::Record;replayCursor=0;toast(L"ROM перезапущен. Запись ввода и контрольных сумм · до 20 минут.");updateMenu();
}
fs::path chooseFile(bool save,const wchar_t* title,const wchar_t* filter,const wchar_t* extension) {
    std::wstring value(32768,0);OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window;
    dialog.lpstrTitle=title;dialog.lpstrFilter=filter;dialog.lpstrDefExt=extension;dialog.lpstrFile=value.data();dialog.nMaxFile=static_cast<DWORD>(value.size());
    dialog.lpstrInitialDir=dataDir.c_str();dialog.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
    modal=true;physicalKeys.fill(false);audio.silence();const bool chosen=(save?GetSaveFileNameW(&dialog):GetOpenFileNameW(&dialog))!=FALSE;
    modal=false;clockReset=true;return chosen?fs::path(value.c_str()):fs::path{};
}
bool startPlayback(const fs::path& path) {
    std::vector<std::uint8_t> bytes;chip8::Replay replay;std::string problem;
    if(!readFile(path,bytes,1024*1024)||!chip8::Replay::decode(bytes,replay,problem)){error(L"Не удалось открыть повтор: "+wide(problem));return false;}
    chip8::Chip8 initial(chip8::Config::preset(chip8::Profile::XoChip));
    if(!initial.loadRom(replay.rom)||!initial.loadState(replay.initial,problem))return false;
    if(!commitGame(replay.rom,path.stem().wstring(),initial.config(),static_cast<int>(replay.hz),{},true,false))return false;
    if(!cpu.loadState(replay.initial,problem))return false;
    scheduler.setHz(replay.hz);scheduler.reset();inputReplay=std::move(replay);replayCursor=0;inputMode=InputMode::Play;
    presenter.reset();refresh();resetHistory();toast(L"Воспроизведение: проверяется каждый кадр.");updateMenu();return true;
}
std::uint16_t keyMask() {
    std::array<bool,16> keys{};
    if(active&&!modal&&!showLibrary) {
        for(unsigned scan=0;scan<physicalKeys.size();++scan)if(physicalKeys[scan]) {
            for(unsigned key=0;key<16;++key)if(bindings[key]==scan)keys[key]=true;
            if(!customControls&&pongControls){if(scan==0x11)keys[1]=true;if(scan==0x1F)keys[4]=true;if(scan==0xC8)keys[12]=true;if(scan==0xD0)keys[13]=true;}
            else if(!customControls) {
                const std::array<unsigned,6> arrows{0xCB,0xCD,0xC8,0xD0,0x39,0x1C};
                for(std::size_t n=0;n<arrows.size();++n)if(scan==arrows[n])keys[gameControls[n]]=true;
            }
        }
        if(virtualHeld>=0&&virtualHeld<16)keys[virtualHeld]=true;
        pads.add(keys);
    }
    std::uint16_t mask=0;for(unsigned key=0;key<16;++key)if(keys[key])mask|=static_cast<std::uint16_t>(1u<<key);return mask;
}
void applyMask(std::uint16_t keys){for(std::uint8_t key=0;key<16;++key)cpu.setKey(key,(keys&(1u<<key))!=0);}
bool beforeInstruction() {
    if(inputMode==InputMode::None&&breakpoints.count(cpu.pc())&&!skipBreakpoint) {
        paused=true;audio.silence();toast(L"Точка останова: "+hex(cpu.pc()));updateMenu();return false;
    }
    skipBreakpoint=false;
    if(debug||traceEnabled){trace.push_back({wide(cpu.disassemble(cpu.pc())),cpu.registers()});if(trace.size()>1024)trace.pop_front();}
    return true;
}
void achievement(const wchar_t* key,const wchar_t* label) {
    if(GetPrivateProfileIntW(L"Achievements",key,0,settings.c_str()))return;
    writeSetting(L"Achievements",key,L"1");toast(L"Достижение: "+std::wstring(label));
}
void advanceVmFrame() {
    if(inputMode==InputMode::Play&&replayCursor>=inputReplay.frames.size()){stopInput(false);paused=true;toast(L"Повтор завершён: все контрольные суммы совпали.");return;}
    const auto mask=inputMode==InputMode::Play?inputReplay.frames[replayCursor].keys:keyMask();applyMask(mask);
    previousRegisters=cpu.registers();
    scheduler.advance(cpu,1.0/60.0,onFrame,onDisplay,beforeInstruction);
    if(paused&&inputMode==InputMode::None)return;
    ++vmFrames;++playFrames;
    const auto digest=inputMode==InputMode::None?0:chip8::stateDigest(cpu.saveState());
    if(inputMode==InputMode::Record){inputReplay.frames.push_back({mask,digest});if(inputReplay.frames.size()>=72000)stopInput();}
    else if(inputMode==InputMode::Play) {
        if(digest!=inputReplay.frames[replayCursor].digest){const auto frame=replayCursor;stopInput(false);paused=true;error(L"Повтор расходится на кадре "+std::to_wstring(frame)+L". Экспортируй отчёт для диагностики.");return;}
        ++replayCursor;
    }
    if(vmFrames%2==0){history.capture(timelineState(),vmFrames);rewindCursor=history.size()?history.size()-1:0;}
    if(vmFrames>=autosaveFrame+1800){saveAutomatic();persistSettings();}
    if(pongControls&&inputMode!=InputMode::Play) {
        const auto& regs=cpu.registers();if(regs[8]>=1||regs[9]>=1)achievement(L"PongPoint",L"Первое очко в Pong");
        if(regs[8]>=3||regs[9]>=3)achievement(L"PongThree",L"Три очка в Pong");
        if(regs[8]>=10||regs[9]>=10)achievement(L"PongTen",L"Десять очков в Pong");
    }
    recordMediaFrame();
}

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
    const int w=512,h=256;auto pixels=screenPixels(w,h);
    Gdiplus::Bitmap image(w,h,w*4,PixelFormat32bppARGB,reinterpret_cast<BYTE*>(pixels.data()));
    return image.Save(path.c_str(),&encoder,nullptr)==Gdiplus::Ok;
}
void screenshot() {
    SYSTEMTIME time{}; GetLocalTime(&time);
    const auto name=L"CHIP8-"+gameId()+L"-"+std::to_wstring(time.wYear)+L"-"+std::to_wstring(time.wMonth)+L"-"+
        std::to_wstring(time.wDay)+L"-"+std::to_wstring(time.wHour)+L"-"+std::to_wstring(time.wMinute)+L"-"+std::to_wstring(time.wSecond)+L".png";
    if(exportScreenshot(dataDir/name)){achievement(L"Screenshot",L"Первый снимок экрана");toast(L"PNG сохранён. Файл → Папка данных.");}
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
    if(searchEdit){SendMessageW(searchEdit,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);MoveWindow(searchEdit,ui(20),ui(66),ui(270),ui(30),TRUE);}
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
constexpr std::array<UINT,6> ToolbarActions{Library,Open,Pause,Restart,Save,Debug};
RECT buttonRect(int index) { return {ui(20+index*110),ui(65),ui(122+index*110),ui(96)}; }
RECT timelineRect() {RECT rect{};GetClientRect(window,&rect);return {ui(20),rect.bottom-ui(110),rect.right-ui(debug||showKeypad?312:20),rect.bottom-ui(87)};}
RECT keypadRect(int key) {RECT r{};GetClientRect(window,&r);constexpr std::array<int,16> order{1,2,3,12,4,5,6,13,7,8,9,14,10,0,11,15};
    const auto pos=std::find(order.begin(),order.end(),key)-order.begin();const int x=r.right-ui(286)+static_cast<int>(pos%4)*ui(63),y=ui(132)+static_cast<int>(pos/4)*ui(57);return {x,y,x+ui(53),y+ui(47)};}
void showCatalog(bool value) {
    showLibrary=value;physicalKeys.fill(false);virtualHeld=-1;if(inputMode==InputMode::None)cpu.releaseKeys();audio.silence();clockReset=true;
    if(value&&window&&!searchEdit){searchEdit=CreateWindowExW(0,L"EDIT",libraryQuery.c_str(),WS_CHILD|WS_BORDER|ES_AUTOHSCROLL,ui(20),ui(66),ui(270),ui(30),window,reinterpret_cast<HMENU>(1900),GetModuleHandleW(nullptr),nullptr);
        SendMessageW(searchEdit,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);SendMessageW(searchEdit,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"Поиск по названию, автору, платформе"));}
    if(searchEdit)ShowWindow(searchEdit,value?SW_SHOW:SW_HIDE);
    if(value){rebuildLibrary();if(searchEdit)SetFocus(searchEdit);}else if(window)SetFocus(window);
    updateMenu();redraw();
}
Gdiplus::Bitmap* coverFor(const LibraryGame& game) {
    if(covers.size()>=64&&!covers.count(game.sha))covers.clear();
    auto& cached=covers[game.sha];if(cached)return cached.get();
    std::error_code ec;
    if(!game.cover.empty()&&fs::is_regular_file(game.cover,ec)){
        Gdiplus::Bitmap original(game.cover.c_str());
        if(original.GetLastStatus()==Gdiplus::Ok){cached=std::make_unique<Gdiplus::Bitmap>(256,128,PixelFormat32bppARGB);Gdiplus::Graphics graphics(cached.get());graphics.Clear(Gdiplus::Color(255,9,17,27));graphics.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
            const double scale=std::min(256.0/original.GetWidth(),128.0/original.GetHeight());const int width=static_cast<int>(original.GetWidth()*scale),height=static_cast<int>(original.GetHeight()*scale);
            graphics.DrawImage(&original,(256-width)/2,(128-height)/2,width,height);return cached.get();}
    }
    std::vector<std::uint8_t> bytes;
    if(game.builtin>=0){const std::array<const std::vector<std::uint8_t>*,4> built{&chip8_demo::Pong2,&chip8_demo::PongClassic,&chip8_demo::Breakout,&chip8_demo::Demo};bytes=*built[game.builtin];}
    else if(!readFile(game.path,bytes,65024))return nullptr;
    if(game.metadata&&!game.metadata->runnable)return nullptr;
    const auto cfg=game.metadata?chip8::Config::fromQuirks(static_cast<chip8::Profile>(game.metadata->profile),static_cast<std::uint8_t>(game.metadata->quirks)):chip8::Config::preset(chip8::Profile::Modern);
    chip8::Chip8 preview(cfg);if(!preview.loadRom(bytes))return nullptr;preview.seedRandom(81);
    chip8::Scheduler clock(game.builtin==0?6000:game.metadata?std::min(game.metadata->hz,12000):700);
    for(int n=0;n<60&&!preview.halted();++n)clock.advance(preview,1.0/60);
    const int width=preview.width(),height=preview.height();cached=std::make_unique<Gdiplus::Bitmap>(width,height,PixelFormat32bppARGB);
    Gdiplus::Rect rect(0,0,width,height);Gdiplus::BitmapData data{};
    if(cached->LockBits(&rect,Gdiplus::ImageLockModeWrite,PixelFormat32bppARGB,&data)!=Gdiplus::Ok){cached.reset();return nullptr;}
    const auto colors=game.metadata?game.metadata->colors:Palettes[0].color;
    for(int y=0;y<height;++y){auto* line=reinterpret_cast<std::uint32_t*>(static_cast<BYTE*>(data.Scan0)+y*data.Stride);for(int x=0;x<width;++x)line[x]=0xFF000000u|colors[preview.display()[static_cast<std::size_t>(y)*128+x]&3];}
    cached->UnlockBits(&data);return cached.get();
}
void paintLibrary(HDC dc,int w,int h) {
    fill(dc,{0,0,w,h},0x0F1722);fill(dc,{0,0,w,ui(112)},0x182334);
    text(dc,L"CHIP-8 STUDIO 3.0",{ui(20),ui(10),ui(350),ui(44)},0xD8F3FF,titleFont);
    text(dc,std::to_wstring(library.size())+L" программ · CHIP-8 / SCHIP / XO",{ui(365),ui(12),w-ui(20),ui(43)},0x91AFC6);
    const std::array<std::wstring,4> filters{L"Все",L"★ Избранное",L"Недавние",L"Вернуться"};
    for(int n=0;n<4;++n){RECT rect{ui(310+n*101),ui(65),ui(403+n*101),ui(97)};fill(dc,rect,n==libraryFilter?0x355069:0x25354B);text(dc,filters[n],rect,0xD8E7F5,nullptr,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
    libraryTiles.clear();const int columns=std::max(1,(w-ui(24))/ui(218)),rows=std::max(1,(h-ui(173))/ui(210));
    const int count=columns*rows,pages=std::max(1,(static_cast<int>(visibleGames.size())+count-1)/count);
    libraryPage=std::clamp(libraryPage,0,pages-1);const int tileW=(w-ui(40)-ui(14)*(columns-1))/columns;
    for(int n=0;n<count;++n){const auto position=static_cast<std::size_t>(libraryPage*count+n);if(position>=visibleGames.size())break;
        const auto index=visibleGames[position];const auto& game=library[index];
        RECT rect{ui(20)+(n%columns)*(tileW+ui(14)),ui(127)+(n/columns)*ui(210),0,0};rect.right=rect.left+tileW;rect.bottom=rect.top+ui(196);libraryTiles.push_back({rect,index});
        const bool compatible=!game.metadata||game.metadata->runnable;fill(dc,rect,compatible?0x1D2B3D:0x232835);
        RECT cover{rect.left+ui(10),rect.top+ui(10),rect.right-ui(10),rect.top+ui(117)};fill(dc,cover,0x09111B);
        if(auto* image=coverFor(game)){Gdiplus::Graphics graphics(dc);graphics.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
            const double scale=std::min(static_cast<double>(cover.right-cover.left)/image->GetWidth(),static_cast<double>(cover.bottom-cover.top)/image->GetHeight());
            const int iw=static_cast<int>(image->GetWidth()*scale),ih=static_cast<int>(image->GetHeight()*scale);
            graphics.DrawImage(image,cover.left+(cover.right-cover.left-iw)/2,cover.top+(cover.bottom-cover.top-ih)/2,iw,ih);
        }else text(dc,compatible?L"ROM / нет обложки":L"Другой тип платформы",cover,0x7F9DB7,nullptr,DT_CENTER|DT_SINGLELINE|DT_VCENTER);
        text(dc,game.title,{rect.left+ui(10),rect.top+ui(120),rect.right-ui(10),rect.top+ui(145)},compatible?0xE2F0FA:0xA9ADB7);
        text(dc,game.authors.empty()?game.platform:game.authors,{rect.left+ui(10),rect.top+ui(144),rect.right-ui(10),rect.top+ui(163)},0x86A4BD);
        std::wstring detail=game.platform+L" · "+std::to_wstring((game.seconds+(game.sha==currentSha?playFrames/60:0))/60)+L" мин";
        if(!compatible)detail=L"Не поддерживается: "+game.platform;
        text(dc,detail,{rect.left+ui(10),rect.top+ui(166),rect.right-ui(36),rect.bottom-ui(5)},0x7892AA);
        text(dc,game.favorite?L"★":L"☆",{rect.right-ui(36),rect.bottom-ui(33),rect.right-ui(5),rect.bottom-ui(4)},game.favorite?0xFFD080:0x9AB2C9,titleFont,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    }
    fill(dc,{0,h-ui(43),w,h},0x182334);
    text(dc,L"Импорт: Ctrl+O / перетаскивание · ПКМ: описание · Колесо: страницы",{ui(20),h-ui(40),w-ui(240),h-ui(8)},0x88A8C2);
    text(dc,L"‹   "+std::to_wstring(libraryPage+1)+L" / "+std::to_wstring(pages)+L"   ›",{w-ui(220),h-ui(40),w-ui(20),h-ui(8)},0xD8E7F5,nullptr,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    if(visibleGames.empty())text(dc,L"По этому запросу ничего не найдено.",{ui(20),ui(150),w-ui(20),ui(210)},0x86A4BD);
}
void paint(HDC destination) {
    RECT client{}; GetClientRect(window,&client);
    const int w=client.right,h=client.bottom;
    if(!back.ensure(destination,w,h)) { FillRect(destination,&client,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH))); return; }
    std::fill_n(back.pixels(),static_cast<std::size_t>(w)*h,0x101722u);
    const HDC dc=back.dc();
    if(showLibrary&&!fullscreen){paintLibrary(dc,w,h);BitBlt(destination,0,0,w,h,dc,0,0,SRCCOPY);return;}
    RECT area{ui(20),ui(116),w-ui(debug||showKeypad?312:20),h-ui(122)};
    if(fullscreen)area={0,0,w,h};
    const int logicalW=rewindPreview?rewindW:presenter.width(),logicalH=rewindPreview?rewindH:presenter.height();
    const int zoom=std::max(1,static_cast<int>(std::min((area.right-area.left)/logicalW,(area.bottom-area.top)/logicalH)));
    const int originX=area.left+((area.right-area.left)-logicalW*zoom)/2;
    const int originY=area.top+((area.bottom-area.top)-logicalH*zoom)/2;
    const int screenW=logicalW*zoom,screenH=logicalH*zoom;const auto pixels=screenPixels(screenW,screenH);
    for(int y=0;y<screenH;++y)if(originY+y>=0&&originY+y<h&&originX>=0&&originX+screenW<=w)
        std::copy_n(pixels.data()+static_cast<std::size_t>(y)*screenW,screenW,back.pixels()+static_cast<std::size_t>(originY+y)*w+originX);
    if(!fullscreen) {
        fill(dc,{0,0,w,ui(106)},0x182334);
        text(dc,L"CHIP-8 STUDIO 3.0",{ui(20),ui(9),ui(250),ui(44)},0xD8F3FF,titleFont);
        text(dc,romName,{ui(265),ui(12),w-ui(20),ui(42)},0xAFC1D5);
        const std::array<std::wstring,6> labels{L"Библиотека",L"Открыть ROM",paused?L"Продолжить":L"Пауза",L"Перезапуск",L"Сохранить "+std::to_wstring(slot+1),debug?L"Скрыть отладку":L"Отладка"};
        for(int n=0;n<6;++n) {
            const auto rect=buttonRect(n); fill(dc,rect,0x27384E);
            text(dc,labels[n],rect,0xD8E7F5,nullptr,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
        }
        fill(dc,{0,h-ui(78),w,h},0x182334);
        std::wstring status=wide(chip8::profileName(cpu.config().profile))+L"   ·   "+std::to_wstring(static_cast<int>(scheduler.hz()))+
            L" инстр/с   ·   "+std::to_wstring(cpu.width())+L"×"+std::to_wstring(cpu.height())+L"   ·   "+(smooth?L"Плавный":stable?L"Стабильный":L"Оригинал");
        if(crt)status+=L" · CRT "+std::to_wstring(intensity)+L"%";
        if(gif.active()||mp4.active())status+=L" · REC";
        if(inputMode==InputMode::Record)status+=L" · Ввод REC";
        if(inputMode==InputMode::Play)status+=L" · Повтор "+std::to_wstring(replayCursor);
        if(showMetrics)status+=L" · "+std::to_wstring(static_cast<int>(measuredFps))+L" FPS · CPU "+std::to_wstring(static_cast<int>(measuredCpu))+L"%";
        if(paused)status+=L"   ·   ПАУЗА";
        else if(!running()&&!cpu.halted())status+=L"   ·   Автопауза";
        if(muted)status+=L"   ·   Без звука";
        text(dc,status,{ui(20),h-ui(75),w-ui(20),h-ui(52)},0x9CBAD2);
        const auto controls=customControls?L"Управление переназначено для этой игры · Эмуляция → Переназначить 16 клавиш":pongControls?L"Pong: W/S — слева, ↑/↓ — справа. Два игрока, без ИИ.":L"Клавиатура: 1 2 3 4 / Q W E R / A S D F / Z X C V · также стрелки и пробел для известных ROM.";
        text(dc,controls,{ui(20),h-ui(53),w-ui(20),h-ui(31)},0xD3E5F3);
        std::wstring help=L"P — пауза  ·  F1 — перезапуск  ·  F5/F8 — сохранить/загрузить  ·  F11 — весь экран";
        if(cpu.halted())help=cpu.error().empty()?L"Программа завершила выполнение (00FD). F1 — начать заново.":wide(cpu.error());
        else if(Clock::now()<noticeUntil)help=notice;
        text(dc,help,{ui(20),h-ui(31),w-ui(20),h-ui(6)},cpu.error().empty()?0x7F9DB7:0xFFAD91);
        const auto timeline=timelineRect();fill(dc,timeline,0x263B50);
        if(history.size()>1){const double fraction=static_cast<double>(rewinding?rewindCursor:history.size()-1)/(history.size()-1);fill(dc,{timeline.left,timeline.top,timeline.left+static_cast<LONG>((timeline.right-timeline.left)*fraction),timeline.bottom},rewinding?0xB88B45:0x3B728A);}
        const auto span=history.size()>1?(history.frame(history.size()-1)-history.frame(0))/60:0;
        text(dc,L"↶  "+std::to_wstring(span)+L" с · Backspace: держать для перемотки · Шкалу можно перетаскивать",timeline,0xD5E8F5,nullptr,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
        if(showKeypad&&!debug){const int left=w-ui(292);fill(dc,{left-ui(10),ui(106),w,h-ui(115)},0x141F2E);
            text(dc,L"ВИРТУАЛЬНАЯ КЛАВИАТУРА",{left,ui(108),w-ui(12),ui(131)},0x9CBAD2);
            for(int key=0;key<16;++key){const auto r=keypadRect(key);fill(dc,r,cpu.isKeyPressed(static_cast<std::uint8_t>(key))?0x487C8D:0x293D55);text(dc,hex(key,1),r,0xD8F3FF,titleFont,DT_CENTER|DT_SINGLELINE|DT_VCENTER);}
            text(dc,L"Удерживай кнопку мышью",{left,ui(380),w-ui(12),ui(410)},0x7892AA);
        }
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
                bool changed=false;for(int column=0;column<4;++column)changed=changed||previousRegisters[row*4+column]!=cpu.registers()[row*4+column];
                line(value,changed?0xFFD080:0xAFC1D5);
            }
            y+=ui(9); line(L"F10: инструкция · F9: кадр",0xD8F3FF);
            line(L"Точки останова: "+std::to_wstring(breakpoints.size()));
            y+=ui(9);
            std::uint16_t address=cpu.pc();
            for(int n=0;n<6;++n) {
                const auto decoded=cpu.disassemble(address); line(wide(decoded),n==0?0x98F5AD:0x7F9DB7);
                const auto& mem=cpu.memory();
                const bool longOp=cpu.config().xo()&&static_cast<std::size_t>(address)+1<cpu.memoryLimit()&&mem[address]==0xF0&&mem[address+1]==0;
                address=static_cast<std::uint16_t>(address+(longOp?4:2));
            }
            y+=ui(9);line(L"Память "+hex(memoryAddress)+L" · PgUp/PgDn");
            for(int row=0;row<4;++row) {
                std::wstring values=hex(static_cast<std::uint16_t>(memoryAddress+row*8))+L": ";
                for(int n=0;n<8;++n) { const auto offset=static_cast<std::size_t>(memoryAddress)+row*8+n; values+=(offset<cpu.memoryLimit()?hex(cpu.memory()[offset],2):L"--")+L" "; }
                line(values);
            }
            RestoreDC(dc,savedDc);
        }
    }
    if(remapIndex>=0){fill(dc,{ui(30),ui(120),w-ui(30),ui(245)},0x263B50);text(dc,L"Клавиша CHIP-8 "+hex(remapIndex,1)+L": нажми физическую клавишу",{ui(50),ui(139),w-ui(50),ui(184)},0xD8F3FF,titleFont);
        text(dc,L"Esc — отменить. P, M, Tab, Backspace и функциональные клавиши зарезервированы.",{ui(50),ui(188),w-ui(50),ui(228)},0xAFC1D5);}
    BitBlt(destination,0,0,w,h,dc,0,0,SRCCOPY);
}

HMENU makeMenu() {
    HMENU bar=CreateMenu();
    auto popup=[&](const wchar_t* name){auto m=CreatePopupMenu();AppendMenuW(bar,MF_POPUP,reinterpret_cast<UINT_PTR>(m),name);return m;};
    auto add=[](HMENU m,UINT id,const wchar_t* label){AppendMenuW(m,MF_STRING,id,label);};
    auto file=popup(L"Файл");add(file,Library,L"Библиотека игр\tCtrl+L");add(file,Open,L"Открыть / добавить ROM…\tCtrl+O");add(file,ImportFolder,L"Импортировать папку ROM…");add(file,Restart,L"Перезапустить текущий ROM\tF1");
    add(file,Save,L"Сохранить состояние\tF5"); add(file,Load,L"Загрузить состояние\tF8"); add(file,Screenshot,L"Снимок PNG\tF6");
    HMENU slots=CreatePopupMenu();for(unsigned n=0;n<10;++n)add(slots,Slot0+n,(L"Слот "+std::to_wstring(n+1)).c_str());AppendMenuW(file,MF_POPUP,reinterpret_cast<UINT_PTR>(slots),L"Текущий слот для этой игры");
    add(file,Autosave,L"Автосохранение каждые 30 с и при выходе");add(file,ResumeAuto,L"Продолжать с автосохранения");
    add(file,ExportSettings,L"Экспорт настроек, ROM и сохранений…");add(file,ImportSettings,L"Импорт настроек, ROM и сохранений…");add(file,Portable,L"Портативный режим (перенести данные)");
    add(file,Folder,L"Папка данных и снимков"); AppendMenuW(file,MF_SEPARATOR,0,nullptr); add(file,Exit,L"Выход\tAlt+F4");
    auto games=popup(L"Игры"); add(games,Pong2,L"Pong — два игрока (встроен)"); add(games,PongClassic,L"Pong — исходный ROM");
    add(games,Breakout,L"Breakout (встроен)"); add(games,Demo,L"Анимированная демонстрация");
    add(games,Favorite,L"Добавить / убрать текущую игру из избранного");add(games,Favorites,L"Избранные игры");add(games,Recent,L"Последние игры");add(games,Achievements,L"Локальные достижения");
    auto emulation=popup(L"Эмуляция"); add(emulation,Pause,L"Пауза\tP"); add(emulation,Step,L"Одна инструкция\tF10"); add(emulation,FrameStep,L"Один кадр\tF9");
    add(emulation,Rewind,L"Назад на секунду (пауза)");
    HMENU duration=CreatePopupMenu();add(duration,Rewind30,L"30 секунд");add(duration,Rewind60,L"60 секунд");add(duration,Rewind120,L"120 секунд");AppendMenuW(emulation,MF_POPUP,reinterpret_cast<UINT_PTR>(duration),L"Длина истории (до 64 МБ)");
    add(emulation,Turbo,L"Ускорение 4× (также удерживай Tab)");add(emulation,Slow,L"Замедление 0,25× (также удерживай Ctrl)");
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
    add(emulation,Remap,L"Переназначить 16 клавиш текущей игры…");add(emulation,Keypad,L"Виртуальная клавиатура");
    auto view=popup(L"Экран");add(view,Exact,L"Оригинал: точное состояние");add(view,Stable,L"Стабильный: стабилизация XOR");add(view,Smooth,L"Плавный: стабилизация и послесвечение");
    add(view,Crt,L"CRT: линии, свечение и виньетка");add(view,CrtLess,L"CRT: уменьшить интенсивность");add(view,CrtMore,L"CRT: увеличить интенсивность");
    HMENU colors=CreatePopupMenu(); for(unsigned n=0;n<Palettes.size();++n)add(colors,Palette0+n,Palettes[n].name);
    AppendMenuW(view,MF_POPUP,reinterpret_cast<UINT_PTR>(colors),L"Палитра");add(view,RomPalette,L"Цвета из базы ROM");add(view,Metrics,L"Счётчики кадров и нагрузки");add(view,Fullscreen,L"Полный экран\tF11");add(view,Debug,L"Панель отладки\tF12");add(view,Mute,L"Отключить звук\tM");
    auto record=popup(L"Запись");add(record,Gif,L"Начать GIF (256×128, 30 FPS)");add(record,Mp4,L"Начать MP4 со звуком (512×256, 30 FPS)");add(record,StopMedia,L"Завершить GIF / MP4");AppendMenuW(record,MF_SEPARATOR,0,nullptr);
    add(record,RecordInput,L"Записать ввод (перезапустить ROM)");add(record,PlayInput,L"Открыть повтор ввода…");add(record,StopInput,L"Завершить запись / повтор ввода");
    auto diagnostics=popup(L"Диагностика");add(diagnostics,Breakpoint,L"Добавить / убрать точку останова по адресу…");add(diagnostics,ClearBreakpoints,L"Удалить все точки останова");
    add(diagnostics,Memory,L"Показать память по адресу…");add(diagnostics,Compare,L"Сравнение экранов: PNG (слева — оригинал)");add(diagnostics,Report,L"Экспортировать отчёт и журнал инструкций");
    auto help=popup(L"Справка"); add(help,Help,L"Управление и совместимость"); add(help,About,L"О программе");
    return bar;
}
unsigned scanCode(LPARAM data) { return static_cast<unsigned>((data>>16)&255)|((data&(1L<<24))?128u:0u); }
void updateKeys() {
    if(inputMode==InputMode::None&&!rewinding&&remapIndex<0)applyMask(keyMask());
}
void releaseInput(){physicalKeys.fill(false);virtualHeld=-1;if(inputMode==InputMode::None)cpu.releaseKeys();}
struct Prompt {HWND edit=nullptr;bool done=false,accepted=false;std::wstring value;};
LRESULT CALLBACK promptProc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
    auto* data=reinterpret_cast<Prompt*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(message==WM_NCCREATE){data=static_cast<Prompt*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(data));}
    if(data&&message==WM_COMMAND&&(LOWORD(wp)==IDOK||LOWORD(wp)==IDCANCEL)) {
        data->accepted=LOWORD(wp)==IDOK;
        if(data->accepted){const int length=GetWindowTextLengthW(data->edit);data->value.resize(static_cast<std::size_t>(length)+1);GetWindowTextW(data->edit,data->value.data(),length+1);data->value.resize(length);}
        DestroyWindow(hwnd);return 0;
    }
    if(data&&message==WM_CLOSE){DestroyWindow(hwnd);return 0;}
    if(data&&message==WM_DESTROY){data->done=true;return 0;}
    return DefWindowProcW(hwnd,message,wp,lp);
}
std::optional<std::wstring> prompt(const wchar_t* title,const std::wstring& initial) {
    WNDCLASSW klass{};klass.lpfnWndProc=promptProc;klass.hInstance=GetModuleHandleW(nullptr);klass.lpszClassName=L"CHIP8StudioPrompt";
    klass.hCursor=LoadCursorW(nullptr,IDC_ARROW);klass.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);RegisterClassW(&klass);
    RECT parent{};GetWindowRect(window,&parent);Prompt data;
    modal=true;releaseInput();audio.silence();EnableWindow(window,FALSE);
    const HWND dialog=CreateWindowExW(WS_EX_DLGMODALFRAME|WS_EX_CONTROLPARENT,klass.lpszClassName,title,WS_POPUP|WS_CAPTION|WS_SYSMENU,
        parent.left+(parent.right-parent.left-ui(410))/2,parent.top+(parent.bottom-parent.top-ui(160))/2,ui(410),ui(160),window,nullptr,klass.hInstance,&data);
    if(!dialog){EnableWindow(window,TRUE);modal=false;return std::nullopt;}
    data.edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",initial.c_str(),WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,ui(18),ui(18),ui(366),ui(29),dialog,nullptr,klass.hInstance,nullptr);
    const HWND yes=CreateWindowW(L"BUTTON",L"Применить",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,ui(157),ui(70),ui(110),ui(30),dialog,reinterpret_cast<HMENU>(IDOK),klass.hInstance,nullptr);
    const HWND no=CreateWindowW(L"BUTTON",L"Отмена",WS_CHILD|WS_VISIBLE|WS_TABSTOP,ui(276),ui(70),ui(108),ui(30),dialog,reinterpret_cast<HMENU>(IDCANCEL),klass.hInstance,nullptr);
    for(const auto item:{data.edit,yes,no})SendMessageW(item,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
    ShowWindow(dialog,SW_SHOW);SetFocus(data.edit);SendMessageW(data.edit,EM_SETSEL,0,-1);
    MSG message{};while(!data.done&&GetMessageW(&message,nullptr,0,0)>0){if(!IsDialogMessageW(dialog,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}
    EnableWindow(window,TRUE);SetForegroundWindow(window);modal=false;clockReset=true;
    return data.accepted?std::optional<std::wstring>(data.value):std::nullopt;
}
std::optional<std::uint16_t> addressPrompt(const wchar_t* title,std::uint16_t initial) {
    const auto input=prompt(title,hex(initial));if(!input)return std::nullopt;
    try {std::size_t read=0;const auto value=std::stoul(*input,&read,16);if(read==input->size()&&value<cpu.memoryLimit())return static_cast<std::uint16_t>(value);}catch(...){}
    error(L"Введи адрес в шестнадцатеричном формате в пределах памяти текущего профиля.");return std::nullopt;
}
void remapKeys() {stopInput();stopMedia();showCatalog(false);remapWasPaused=paused;paused=true;remapIndex=0;pendingBindings.fill(0);releaseInput();audio.silence();refresh();}
void toggleFavorite(std::size_t index) {
    if(index>=library.size())return;
    auto& game=library[index];game.favorite=!game.favorite;
    writeSetting((L"Library."+game.sha).c_str(),L"Favorite",std::to_wstring(game.favorite));rebuildLibrary();redraw();
}
bool launchLibrary(std::size_t index) {
    if(index>=library.size())return false;
    const auto game=library[index];
    if(game.metadata&&!game.metadata->runnable){toast(L"Этот ROM требует "+game.platform+L". Платформа пока не реализована.");return false;}
    if(game.builtin>=0){const std::array<const std::vector<std::uint8_t>*,4> built{&chip8_demo::Pong2,&chip8_demo::PongClassic,&chip8_demo::Breakout,&chip8_demo::Demo};
        return commitGame(*built[game.builtin],game.title,chip8::Config::preset(chip8::Profile::Modern),game.builtin==0?6000:game.builtin==3?1500:700);}
    return loadPath(game.path);
}
void importFolder() {
    BROWSEINFOW info{};info.hwndOwner=window;info.lpszTitle=L"Папка с ROM: .ch8 / .sc8 / .xo8 / .rom";info.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;
    const auto hr=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);modal=true;releaseInput();audio.silence();
    PIDLIST_ABSOLUTE selected=SHBrowseForFolderW(&info);wchar_t path[MAX_PATH]{};bool picked=selected&&SHGetPathFromIDListW(selected,path);
    if(selected)CoTaskMemFree(selected);
    if(SUCCEEDED(hr))CoUninitialize();
    modal=false;clockReset=true;if(!picked)return;
    std::error_code ec;unsigned count=0,examined=0;
    for(const auto& entry:fs::recursive_directory_iterator(path,fs::directory_options::skip_permission_denied,ec)) {
        if(++examined>20000||count>=2000)break;
        if(entry.is_symlink(ec)||!entry.is_regular_file(ec))continue;
        const auto ext=lowercase(entry.path().extension().wstring());if(ext!=L".ch8"&&ext!=L".sc8"&&ext!=L".xo8"&&ext!=L".rom")continue;
        if(importRom(entry.path()))++count;
    }
    showCatalog(true);toast(L"Добавлено / найдено ROM: "+std::to_wstring(count));
}
void exportReport() {
    std::ostringstream out;out<<"CHIP-8 Studio 3.0 diagnostic report\nROM: "<<utf8(romName)<<"\nSHA-1: "<<utf8(currentSha)
        <<"\nFingerprint: "<<utf8(gameId())<<"\nProfile: "<<chip8::profileName(cpu.config().profile)<<"\nQuirks: "<<static_cast<int>(cpu.config().quirks())
        <<"\nCPU Hz: "<<scheduler.hz()<<"\nGuest frame: "<<vmFrames<<"\nDisplay: "<<cpu.width()<<'x'<<cpu.height()
        <<"\nInstructions: "<<cpu.instructionCount()<<"\nPC: "<<utf8(hex(cpu.pc()))<<"\nI: "<<utf8(hex(cpu.index()))
        <<"\nDT: "<<static_cast<int>(cpu.delayTimer())<<"\nST: "<<static_cast<int>(cpu.soundTimer())<<"\nError: "<<cpu.error()
        <<"\nPresentation: "<<(smooth?"smooth":stable?"stable":"exact")<<"\nCRT: "<<crt<<" / "<<intensity
        <<"\nReplay mode: "<<static_cast<int>(inputMode)<<" / "<<replayCursor<<"\nRegisters:";
    for(auto reg:cpu.registers())out<<' '<<utf8(hex(reg,2));
    out<<"\nBreakpoints:";for(auto address:breakpoints)out<<' '<<utf8(hex(address));
    out<<"\n\nLast instructions (register values before execution; tracing is active in debug view):\n";
    for(const auto& row:trace){out<<utf8(row.decoded);for(auto reg:row.registers)out<<' '<<utf8(hex(reg,2));out<<'\n';}
    const auto value=out.str();const auto path=outputPath(L"-report.txt");
    if(writeAtomic(path,std::vector<std::uint8_t>(value.begin(),value.end())))toast(L"Отчёт сохранён: "+path.filename().wstring());else error(L"Не удалось сохранить отчёт.");
}
void compareScreens() {
    const auto original=screenPixels(512,256,true,false),presented=screenPixels(512,256,false,false);
    std::vector<std::uint32_t> pixels(1032*256,0xFF263B50);
    for(int y=0;y<256;++y){std::copy_n(original.data()+y*512,512,pixels.data()+y*1032);std::copy_n(presented.data()+y*512,512,pixels.data()+y*1032+520);}
    CLSID encoder{};const auto path=outputPath(L"-comparison.png");Gdiplus::Bitmap image(1032,256,1032*4,PixelFormat32bppARGB,reinterpret_cast<BYTE*>(pixels.data()));
    if(pngEncoder(encoder)&&image.Save(path.c_str(),&encoder,nullptr)==Gdiplus::Ok)toast(L"Сравнение сохранено: слева оригинал, справа представленный кадр.");else error(L"Не удалось сохранить сравнение.");
}
void exportSettings(const fs::path& target) {
    saveAutomatic();persistSettings();WritePrivateProfileStringW(nullptr,nullptr,nullptr,settings.c_str());std::vector<chip8_storage::Entry> entries;
    std::error_code ec;for(const auto& file:fs::recursive_directory_iterator(dataDir,fs::directory_options::skip_permission_denied,ec)) {
        if(file.is_symlink(ec)||!file.is_regular_file(ec))continue;
        const auto name=utf8(file.path().lexically_relative(dataDir).generic_wstring());if(!chip8_storage::safeName(name))continue;
        std::vector<std::uint8_t> bytes;if(!readFile(file.path(),bytes,4*1024*1024)){error(L"Не удалось прочитать настройки для экспорта.");return;}entries.push_back({name,std::move(bytes)});
    }
    const auto packed=chip8_storage::encode(entries);
    if(!packed.empty()&&writeAtomic(target,packed))toast(L"Настройки и сохранения экспортированы.");else error(L"Не удалось создать пакет настроек (предел 64 МБ).");
}
bool importSettings(const fs::path& source) {
    std::vector<std::uint8_t> packed;std::vector<chip8_storage::Entry> entries;
    if(!readFile(source,packed,64*1024*1024)||!chip8_storage::decode(packed,entries)){error(L"Повреждённый или неподдерживаемый пакет настроек.");return false;}
    for(const auto& entry:entries)if(entry.name.starts_with("roms/")&&(entry.bytes.size()<2||entry.bytes.size()>65024||sha1(entry.bytes)!=wide(entry.name.substr(5,40)))){error(L"Контрольная сумма ROM в пакете не совпала.");return false;}
    stopInput();stopMedia();saveAutomatic();persistSettings();
    const auto staging=dataDir/L"import-staging";std::error_code ec;fs::create_directories(staging/L"roms",ec);
    for(const auto& entry:entries)if(!writeAtomic(staging/wide(entry.name),entry.bytes)){error(L"Не удалось подготовить импорт.");return false;}
    for(const auto& entry:entries) {
        const auto destination=dataDir/wide(entry.name);fs::create_directories(destination.parent_path(),ec);
        if(!MoveFileExW((staging/wide(entry.name)).c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){error(L"Не удалось применить импорт. Подготовленные файлы остались в папке данных.");return false;}
    }
    fs::remove_all(staging,ec);WritePrivateProfileStringW(nullptr,nullptr,nullptr,settings.c_str());initializeData();initializeLibrary();
    const auto bytes=romBytes;const auto name=romName;const auto path=romPath;const auto cfg=cpu.config();const int hz=static_cast<int>(scheduler.hz());
    commitGame(bytes,name,cfg,hz,path,false,true);toast(L"Настройки и сохранения импортированы.");return true;
}
void togglePortable() {
    stopInput();stopMedia();saveAutomatic();persistSettings();const auto previous=dataDir;const auto marker=exeDir/L"portable.ini";std::error_code ec;
    if(!portable){if(!writeAtomic(marker,{'C','H','I','P','8',' ','S','t','u','d','i','o',' ','p','o','r','t','a','b','l','e','\n'})){error(L"Не удалось включить портативный режим: нет доступа к папке EXE.");return;}}
    else if(!fs::remove(marker,ec)){error(L"Не удалось отключить портативный режим.");return;}
    initializeData();const auto destination=dataDir;
    if(previous!=destination){fs::copy(previous,destination,fs::copy_options::recursive|fs::copy_options::overwrite_existing|fs::copy_options::skip_symlinks,ec);
        if(ec){if(portable)fs::remove(marker,ec);else writeAtomic(marker,{'1'});dataDir=previous;settings=dataDir/L"settings.ini";portable=previous==exeDir/L"userdata";error(L"Не удалось перенести данные. Текущая папка сохранена.");return;}}
    settings=dataDir/L"settings.ini";initializeData();initializeLibrary();persistSettings();toast(portable?L"Портативный режим: данные в userdata рядом с EXE.":L"Настройки перенесены в локальную папку приложения.");
}
void command(UINT id) {
    if(id>=Profile0&&id<Profile0+4) { changeConfig(chip8::Config::preset(static_cast<chip8::Profile>(id-Profile0))); return; }
    if(id>=Speed0&&id<Speed0+Speeds.size()) {stopInput();stopMedia();scheduler.setHz(Speeds[id-Speed0]);scheduler.reset();resetHistory();clockReset=true;persistSettings();refresh();return;}
    if(id>=Palette0&&id<Palette0+Palettes.size()) {palette=static_cast<int>(id-Palette0);romPalette=false;persistSettings();refresh();return;}
    if(id>=Slot0&&id<Slot0+10){slot=static_cast<int>(id-Slot0);persistSettings();updateMenu();toast(L"Текущий слот: "+std::to_wstring(slot+1));return;}
    if(id>=Quirk0&&id<Quirk0+7) { const auto bits=static_cast<std::uint8_t>(cpu.config().quirks()^(1u<<(id-Quirk0))); changeConfig(chip8::Config::fromQuirks(cpu.config().profile,bits)); return; }
    switch(id) {
    case Open: openFile(); break;
    case Demo: commitGame(chip8_demo::Demo,L"Демонстрация C8",chip8::Config::preset(chip8::Profile::Modern),1500); break;
    case Pong2: commitGame(chip8_demo::Pong2,L"Pong — два игрока",chip8::Config::preset(chip8::Profile::Modern),6000); break;
    case PongClassic: commitGame(chip8_demo::PongClassic,L"Pong — исходный ROM",chip8::Config::preset(chip8::Profile::Modern),700); break;
    case Breakout: commitGame(chip8_demo::Breakout,L"Breakout",chip8::Config::preset(chip8::Profile::Modern),700); break;
    case Restart: restart(); break;
    case Pause:paused=!paused;skipBreakpoint=!paused;clockReset=true;audio.silence();refresh();break;
    case Save: saveSlot(); break;
    case Load: loadSlot(); break;
    case Screenshot: screenshot(); break;
    case Rewind:rewind();if(rewinding){restoreTimeline(rewindCursor>30?rewindCursor-30:0);rewindWasPaused=true;finishRewind();}break;
    case Step:stopInput();stopMedia();paused=true;previousRegisters=cpu.registers();if(debug)trace.push_back({wide(cpu.disassemble(cpu.pc())),cpu.registers()});cpu.step();clockReset=true;audio.silence();refresh();break;
    case FrameStep:stopInput();paused=false;skipBreakpoint=true;advanceVmFrame();paused=true;clockReset=true;audio.silence();refresh(false);break;
    case Debug: debug=!debug; refresh(); break;
    case Fullscreen: toggleFullscreen(); break;
    case Mute: muted=!muted; audio.silence(); persistSettings(); refresh(); break;
    case Stable:stable=true;smooth=false;presenter.reset();persistSettings();refresh();break;
    case Exact:stable=false;smooth=false;crt=false;presenter.reset();persistSettings();refresh();break;
    case Smooth:smooth=true;stable=true;phosphor.fill(0);presenter.reset();persistSettings();refresh();break;
    case Crt:crt=!crt;persistSettings();refresh(false);break;
    case CrtLess:intensity=std::max(0,intensity-10);persistSettings();refresh(false);break;
    case CrtMore:intensity=std::min(100,intensity+10);persistSettings();refresh(false);break;
    case AutoPause: autoPause=!autoPause; clockReset=true; persistSettings(); refresh(); break;
    case Gamepad: gamepad=!gamepad; updateKeys(); persistSettings(); refresh(); break;
    case Library:showCatalog(!showLibrary);break;
    case Favorites:libraryFilter=1;showCatalog(true);break;
    case Recent:libraryFilter=2;showCatalog(true);break;
    case LibraryAll:libraryFilter=0;showCatalog(true);break;
    case Favorite:for(std::size_t n=0;n<library.size();++n)if(library[n].sha==currentSha){toggleFavorite(n);break;}break;
    case ImportFolder:importFolder();break;
    case Remap:remapKeys();break;
    case Keypad:showKeypad=!showKeypad;if(showKeypad)debug=false;persistSettings();refresh(false);break;
    case Autosave:autoSave=!autoSave;persistSettings();break;
    case ResumeAuto:resumeAuto=!resumeAuto;persistSettings();break;
    case RomPalette:romPalette=!romPalette;persistSettings();refresh();break;
    case Gif:startMedia(false);break;
    case Mp4:startMedia(true);break;
    case StopMedia:stopMedia();break;
    case RecordInput:startInputRecording();break;
    case PlayInput:{const auto path=chooseFile(false,L"Открыть повтор ввода",L"CHIP-8 Replay\0*.c8replay\0\0",L"c8replay");if(!path.empty())startPlayback(path);break;}
    case StopInput:stopInput();break;
    case Slow:slowMode=!slowMode;if(slowMode)turboMode=false;break;
    case Turbo:turboMode=!turboMode;if(turboMode)slowMode=false;break;
    case Metrics:showMetrics=!showMetrics;persistSettings();break;
    case Rewind30:case Rewind60:case Rewind120:rewindSeconds=id==Rewind30?30:id==Rewind60?60:120;resetHistory();persistSettings();toast(L"Новая длина истории: "+std::to_wstring(rewindSeconds)+L" с");break;
    case Breakpoint:if(const auto address=addressPrompt(L"Точка останова: HEX-адрес",cpu.pc())){if(breakpoints.count(*address))breakpoints.erase(*address);else breakpoints.insert(*address);debug=true;refresh(false);}break;
    case ClearBreakpoints:breakpoints.clear();toast(L"Точки останова удалены.");break;
    case Memory:if(const auto address=addressPrompt(L"Память: HEX-адрес",memoryAddress)){memoryAddress=*address;debug=true;refresh(false);}break;
    case Report:exportReport();break;
    case Compare:compareScreens();break;
    case ExportSettings:{const auto path=chooseFile(true,L"Экспорт настроек и сохранений",L"CHIP-8 Settings\0*.c8pack\0\0",L"c8pack");if(!path.empty())exportSettings(path);break;}
    case ImportSettings:{const auto path=chooseFile(false,L"Импорт настроек и сохранений",L"CHIP-8 Settings\0*.c8pack\0\0",L"c8pack");if(!path.empty())importSettings(path);break;}
    case Portable:togglePortable();break;
    case Achievements:{std::wstring message=L"Локальные испытания для встроенного Pong:\n\n";
        for(const auto& item:{std::pair<const wchar_t*,const wchar_t*>{L"PongPoint",L"Первое очко"},{L"PongThree",L"Набрать 3 очка"},{L"PongTen",L"Набрать 10 очков"},{L"Screenshot",L"Сделать PNG-снимок"}})message+=(GetPrivateProfileIntW(L"Achievements",item.first,0,settings.c_str())?L"✓ ":L"○ ")+std::wstring(item.second)+L"\n";
        modal=true;releaseInput();audio.silence();MessageBoxW(window,message.c_str(),L"Достижения",MB_OK|MB_ICONINFORMATION);modal=false;clockReset=true;break;}
    case Folder: ShellExecuteW(window,L"open",dataDir.c_str(),nullptr,nullptr,SW_SHOWNORMAL); break;
    case Exit: PostMessageW(window,WM_CLOSE,0,0); break;
    case Help:
        modal=true;audio.silence();releaseInput();
        MessageBoxW(window,L"Pong на двух игроков: W/S — левая ракетка; стрелки ↑/↓ — правая.\n"
            L"Стандартная CHIP-8 клавиатура: 1234 / QWER / ASDF / ZXCV.\nКлавиши работают при русской раскладке.\n\n"
            L"P — пауза, F1 — начать текущий ROM заново.\nF5/F8 — сохранить/загрузить; F6 — PNG.\n"
            L"F9 — кадр; F10 — инструкция; F12 — отладка.\nF11 — полный экран; Esc — выйти из полного экрана.\nBackspace — удерживать для перемотки; M — звук.\nTab — 4×; Ctrl — 0,25×. Ctrl+L — библиотека.\n\n"
            L"Старые игры: профиль CHIP-8 (VIP). Современные: Modern.\n"
            L"Super-CHIP: 128×64, прокрутка и RPL. XO-CHIP: два цветовых слоя, 64 КБ, узорный звук.\n"
            L"Профиль и особенности инструкций перезапускают ROM.\nРасширение .sc8/.xo8 выбирает профиль; для .ch8 выбирай вручную при необходимости.\n\n"
            L"Оригинал показывает точный экран. Стабильный удерживает стёртый XOR-спрайт до перерисовки (до 66,7 мс). Плавный добавляет послесвечение; CRT включается отдельно.\n\n"
            L"GIF/MP4 — до 2 минут виртуального времени. Повтор ввода начинается с перезапуска ROM и проверяет состояние каждого кадра (до 20 минут).\n"
            L"Каталог: 210 уникальных игр, демо и программ из двух коллекций; не все когда-либо выпущенные ROM. Другие платформы отмечены отдельно.",
            L"Управление и совместимость",MB_OK|MB_ICONINFORMATION);
        modal=false; clockReset=true; break;
    case About:
        modal=true;audio.silence();releaseInput();
        MessageBoxW(window,L"CHIP-8 Studio 3.0\n\nC++20 / Win32. Собственное переносимое ядро.\n"
            L"CHIP-8, Modern Super-CHIP, XO-CHIP 1.1.\nCPU и таймеры 60 Гц синхронизированы.\n"
            L"Библиотека, CRT, 10 слотов, автосохранения, перемотка до 120 с, GIF/MP4 и проверяемые повторы ввода.\n\n"
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
        if(!active){releaseInput();finishRewind();audio.silence();}
        return 0;
    case WM_KILLFOCUS:releaseInput();finishRewind();return 0;
    case WM_ENTERMENULOOP:modal=true;releaseInput();finishRewind();audio.silence();return 0;
    case WM_EXITMENULOOP: modal=false;clockReset=true; return 0;
    case WM_KEYDOWN: {
        const bool repeat=(lp&(1L<<30))!=0;
        const unsigned scan=scanCode(lp);
        if(remapIndex>=0) {
            if(repeat)return 0;
            if(wp==VK_ESCAPE){remapIndex=-1;paused=remapWasPaused;clockReset=true;refresh();return 0;}
            if(scan==0||scan>=256||scan==0x19||scan==0x32||wp==VK_BACK||wp==VK_TAB||wp==VK_CONTROL||(wp>=VK_F1&&wp<=VK_F24)){toast(L"Клавиша зарезервирована. Выбери другую.");redraw();return 0;}
            if(std::find(pendingBindings.begin(),pendingBindings.begin()+remapIndex,scan)!=pendingBindings.begin()+remapIndex){toast(L"Эта клавиша уже назначена.");return 0;}
            pendingBindings[remapIndex++]=scan;
            if(remapIndex==16){bindings=pendingBindings;customControls=true;remapIndex=-1;paused=remapWasPaused;clockReset=true;persistSettings();toast(L"Управление сохранено для текущего ROM.");}
            redraw();return 0;
        }
        if((GetKeyState(VK_CONTROL)&0x8000)&&wp=='O') { if(!repeat)command(Open);return 0; }
        if((GetKeyState(VK_CONTROL)&0x8000)&&wp=='L'){if(!repeat)command(Library);return 0;}
        if(wp==VK_BACK){if(!repeat&&!showLibrary)rewind();return 0;}
        if(debug&&(wp==VK_PRIOR||wp==VK_NEXT)){const int address=static_cast<int>(memoryAddress)+(wp==VK_NEXT?32:-32);memoryAddress=static_cast<std::uint16_t>(std::clamp(address,0,static_cast<int>(cpu.memoryLimit())-32));redraw();return 0;}
        UINT action=0;
        switch(wp) {
        case VK_F1:action=Restart;break; case VK_F5:action=Save;break; case VK_F6:action=Screenshot;break;
        case VK_F8:action=Load;break;case VK_F9:action=FrameStep;break;case VK_F10:action=Step;break;
        case VK_F11:action=Fullscreen;break;case VK_F12:action=Debug;break;
        case VK_ESCAPE:if(!repeat){if(fullscreen)toggleFullscreen();else if(showLibrary)showCatalog(false);}return 0;
        }
        if(scan==0x19)action=Pause; // physical P, independent of keyboard layout
        if(scan==0x32)action=Mute;
        if(action) { if(!repeat)command(action);return 0; }
        if(scan<physicalKeys.size())physicalKeys[scan]=true;
        updateKeys(); return 0;
    }
    case WM_KEYUP:{if(wp==VK_BACK){finishRewind();return 0;}const unsigned scan=scanCode(lp);if(scan<physicalKeys.size())physicalKeys[scan]=false;updateKeys();return 0;}
    case WM_LBUTTONDOWN:{
        POINT point{static_cast<short>(LOWORD(lp)),static_cast<short>(HIWORD(lp))};
        if(!showLibrary&&!fullscreen){const auto timeline=timelineRect();if(PtInRect(&timeline,point)&&history.size()>1){rewind();timelineDragging=true;SetCapture(hwnd);const auto fraction=std::clamp(static_cast<double>(point.x-timeline.left)/(timeline.right-timeline.left),0.0,1.0);restoreTimeline(static_cast<std::size_t>(fraction*(history.size()-1)));return 0;}
            if(showKeypad&&!debug)for(int key=0;key<16;++key){const auto rect=keypadRect(key);if(PtInRect(&rect,point)){virtualHeld=key;SetCapture(hwnd);updateKeys();redraw();return 0;}}
        }return 0;
    }
    case WM_MOUSEMOVE:if(timelineDragging&&history.size()>1){const auto timeline=timelineRect();const int x=static_cast<short>(LOWORD(lp));const auto fraction=std::clamp(static_cast<double>(x-timeline.left)/(timeline.right-timeline.left),0.0,1.0);restoreTimeline(static_cast<std::size_t>(fraction*(history.size()-1)));}return 0;
    case WM_LBUTTONUP:{
        if(timelineDragging){finishRewind();ReleaseCapture();return 0;}
        if(virtualHeld>=0){virtualHeld=-1;ReleaseCapture();updateKeys();redraw();return 0;}
        POINT point{static_cast<short>(LOWORD(lp)),static_cast<short>(HIWORD(lp))};
        if(showLibrary&&!fullscreen){
            for(int n=0;n<4;++n){const RECT rect{ui(310+n*101),ui(65),ui(403+n*101),ui(97)};if(PtInRect(&rect,point)){if(n==3)showCatalog(false);else{libraryFilter=n;rebuildLibrary();redraw();}return 0;}}
            RECT client{};GetClientRect(hwnd,&client);if(point.y>=client.bottom-ui(43)&&point.x>client.right-ui(220)){libraryPage+=point.x<client.right-ui(120)?-1:1;redraw();return 0;}
            for(const auto& tile:libraryTiles)if(PtInRect(&tile.first,point)){if(point.x>tile.first.right-ui(40)&&point.y>tile.first.bottom-ui(35))toggleFavorite(tile.second);else launchLibrary(tile.second);return 0;}
        }else if(!fullscreen)for(int n=0;n<6;++n){const auto rect=buttonRect(n);if(PtInRect(&rect,point))command(ToolbarActions[n]);}
        return 0;
    }
    case WM_CAPTURECHANGED:if(timelineDragging)finishRewind();if(virtualHeld>=0){virtualHeld=-1;updateKeys();}return 0;
    case WM_MOUSEWHEEL:if(showLibrary){libraryPage+=GET_WHEEL_DELTA_WPARAM(wp)<0?1:-1;redraw();}return 0;
    case WM_RBUTTONUP:if(showLibrary){POINT point{static_cast<short>(LOWORD(lp)),static_cast<short>(HIWORD(lp))};for(const auto& tile:libraryTiles)if(PtInRect(&tile.first,point)){const auto& game=library[tile.second];
        const auto value=game.title+L"\n"+game.authors+L"\nПлатформа: "+game.platform+L"\nПоследний запуск: "+localDate(game.last)+L"\nВремя игры: "+std::to_wstring((game.seconds+(game.sha==currentSha?playFrames/60:0))/60)+L" мин\nSHA-1: "+game.sha+L"\n\n"+game.description;
        modal=true;releaseInput();MessageBoxW(hwnd,value.c_str(),L"О программе / игре",MB_OK|MB_ICONINFORMATION);modal=false;clockReset=true;break;}}return 0;
    case WM_COMMAND:if(LOWORD(wp)==1900&&HIWORD(wp)==EN_CHANGE){const int length=GetWindowTextLengthW(searchEdit);libraryQuery.resize(static_cast<std::size_t>(length)+1);GetWindowTextW(searchEdit,libraryQuery.data(),length+1);libraryQuery.resize(length);rebuildLibrary();redraw();}else command(LOWORD(wp));return 0;
    case WM_DROPFILES: {
        auto drop=reinterpret_cast<HDROP>(wp);const auto count=DragQueryFileW(drop,0xFFFFFFFF,nullptr,0);std::vector<fs::path> paths;
        for(UINT index=0;index<std::min<UINT>(count,2000);++index){const auto length=DragQueryFileW(drop,index,nullptr,0);std::wstring file(static_cast<std::size_t>(length)+1,0);DragQueryFileW(drop,index,file.data(),length+1);paths.emplace_back(file.c_str());}DragFinish(drop);
        if(paths.size()==1)loadPath(paths.front());else{unsigned added=0;for(const auto& path:paths)if(importRom(path))++added;showCatalog(true);toast(L"Импортировано ROM: "+std::to_wstring(added));}
        return 0;
    }
    case WM_ERASEBKGND:return 1;
    case WM_PAINT: { PAINTSTRUCT ps{};const HDC dc=BeginPaint(hwnd,&ps);paint(dc);EndPaint(hwnd,&ps);return 0; }
    case WM_CLOSE:finishRewind();stopInput();stopMedia();saveAutomatic();persistSettings();saveFlags();DestroyWindow(hwnd);return 0;
    case WM_DESTROY:
        closing=true;audio.stop();physicalKeys.fill(false);cpu.releaseKeys();covers.clear();back.clear();PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(hwnd,message,wp,lp);
}

bool createWindow(HINSTANCE instance,int show) {
    using SystemDpi=UINT(WINAPI*)();const auto systemDpi=procedure<SystemDpi>(GetModuleHandleW(L"user32.dll"),"GetDpiForSystem");if(systemDpi)dpi=systemDpi();
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.hInstance=instance;wc.lpfnWndProc=wndProc;
    wc.lpszClassName=ClassName;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(IDI_APP_ICON));wc.hIconSm=wc.hIcon;
    if(!RegisterClassExW(&wc))return false;
    RECT rect{0,0,ui(1050),ui(710)};AdjustWindowRectEx(&rect,WS_OVERLAPPEDWINDOW,TRUE,0);
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
    dataDir=fs::absolute(directory);settings=dataDir/L"settings.ini";initializeLibrary();
    bool ok=true;std::ostringstream details;
    auto require=[&](bool condition,const char* label="native path"){ok=condition&&ok;details<<(condition?"PASS ":"FAIL ")<<label<<'\n';};
    for(const auto action:{Demo,Pong2,PongClassic,Breakout}) {
        command(action);restart();for(int frame=0;frame<120;++frame)advanceVmFrame();require(!cpu.halted(),"builtin ROM");
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
    restart();for(int frame=0;frame<120;++frame)advanceVmFrame();const auto earlier=cpu.saveState();
    for(int frame=0;frame<240;++frame)advanceVmFrame();
    require(history.size()>100,"continuous timeline captures");
    rewind();std::size_t index=0;while(index<history.size()&&history.frame(index)!=120)++index;
    require(restoreTimeline(index)&&cpu.saveState()==earlier,"timeline state and scheduler restore");finishRewind();
    require(history.frame(history.size()-1)==120,"timeline branching discards future");
    for(slot=0;slot<10;++slot){saveSlot();std::vector<std::uint8_t> bytes;require(readFile(statePath(),bytes,80000),"per-ROM save slot");}slot=0;
    startInputRecording();for(int frame=0;frame<120;++frame){physicalKeys[0x11]=frame%20<10;advanceVmFrame();}
    const auto recorded=cpu.saveState();stopInput();const auto replayPath=readSetting(L"App",L"LastReplay");
    require(!replayPath.empty()&&startPlayback(replayPath),"replay load through native front end");
    for(int frame=0;frame<120&&inputMode==InputMode::Play;++frame)advanceVmFrame();
    require(replayCursor==120&&cpu.saveState()==recorded,"replay matches each frame and final state");stopInput(false);
    saveAutomatic();const auto autoState=cpu.saveState();const auto bytes=romBytes;const auto name=romName;
    require(commitGame(bytes,name,cpu.config(),static_cast<int>(scheduler.hz()),{},false,true)&&cpu.saveState()==autoState,"automatic resume");
    const auto exactState=cpu.saveState();command(Smooth);command(Crt);require(cpu.saveState()==exactState,"CRT/phosphor do not modify guest state");
    require(exportScreenshot(directory/L"crt.png"));command(Exact);
    debug=false;showKeypad=true;redraw();const auto key=keypadRect(1);const LPARAM point=MAKELPARAM((key.left+key.right)/2,(key.top+key.bottom)/2);
    SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,point);require(cpu.isKeyPressed(1),"virtual keypad press");
    SendMessageW(window,WM_LBUTTONUP,0,point);require(!cpu.isKeyPressed(1),"virtual keypad release");showKeypad=false;
    const auto originalBindings=bindings;remapKeys();
    for(int n=0;n<16;++n)SendMessageW(window,WM_KEYDOWN,'K',static_cast<LPARAM>(originalBindings[15-n]<<16));
    require(remapIndex==-1&&customControls&&bindings[0]==originalBindings[15],"keyboard remap capture and persistence");
    customControls=false;bindings=originalBindings;persistSettings();
    writeSetting(L"App",L"TransferTest",L"31415");exportSettings(directory/L"settings.c8pack");require(fs::exists(directory/L"settings.c8pack"),"settings export");
    writeSetting(L"App",L"TransferTest",L"0");require(importSettings(directory/L"settings.c8pack")&&readSetting(L"App",L"TransferTest")==L"31415","settings import restores values");
    for(const auto& program:chip8_catalog::Games){std::vector<std::uint8_t> bytes;
        require(readFile(assetPath(program.path),bytes,65024)&&sha1(bytes)==wide(program.sha1),"catalog file fingerprint");}
    showCatalog(true);require(visibleGames.size()>=210,"offline catalog size");require(captureWindow(directory/L"library.png"));
    const auto before=library[0].favorite;toggleFavorite(0);require(library[0].favorite!=before,"library favorite");toggleFavorite(0);
    libraryFilter=2;rebuildLibrary();require(!visibleGames.empty(),"recent games");libraryFilter=0;showCatalog(false);
    const std::vector<std::uint8_t> beep{0x60,0x0F,0xF0,0x18,0x61,0x0A,0xF1,0x29,0x62,0x01,0x63,0x01,0xD2,0x35,0x12,0x00};
    commitGame(beep,L"Audio/video verification",chip8::Config::preset(chip8::Profile::Modern),700,{},true,false);
    palette=0;romPalette=false;stable=smooth=crt=false;refresh();
    require(gif.start(directory/L"capture.gif"),"GIF writer start");
    const bool started=mp4.start(directory/L"capture.mp4");require(started,"Media Foundation H264/AAC writer start");
    if(!started)details<<"MP4 HRESULT: "<<utf8(hex(static_cast<std::uint32_t>(mp4.error()),8))<<'\n';
    for(int frame=0;frame<60;++frame)advanceVmFrame();
    require(gif.frames()==30,"GIF frame count");require(mp4.frames()==30,"MP4 frame count");
    require(gif.finish(),"GIF finalization");require(mp4.finish(),"MP4 finalization");require(exportScreenshot(directory/L"media-last.png"));
    compareScreens();debug=true;for(int n=0;n<2;++n)advanceVmFrame();exportReport();require(!trace.empty(),"instruction trace");
    ok=ok&&smokeErrors.empty();std::ofstream report(directory/L"smoke-result.txt");report<<(ok?"PASS":"FAIL")<<"\n"<<details.str();for(const auto& message:smokeErrors)report<<"ERROR "<<utf8(message)<<'\n';report.close();
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
        exeDir=executableDirectory();initializeData();initializeLibrary();
        if(!createWindow(instance,show)) { error(L"Не удалось создать окно.");Gdiplus::GdiplusShutdown(gdiplusToken);return 1; }
        int argc=0;LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);
        if(argv&&argc>=3&&wcscmp(argv[1],L"--smoke-test")==0) {
            smokeMode=true;
            const auto directory=fs::path(argv[2]);LocalFree(argv);
            const int result=smokeTest(directory);
            for(auto f:{font,titleFont,monoFont})if(f)DeleteObject(f);
            Gdiplus::GdiplusShutdown(gdiplusToken);return result;
        }
        const auto previousGame=readSetting(L"App",L"LastGame");
        commitGame(chip8_demo::Pong2,L"Pong — два игрока",chip8::Config::preset(chip8::Profile::Modern),6000,{},false);
        if(argv&&argc>1){const fs::path path(argv[1]);if(lowercase(path.extension().wstring())==L".c8replay")startPlayback(path);else loadPath(path);}
        else if(resumeAuto&&!previousGame.empty())for(std::size_t n=0;n<library.size();++n)if(library[n].sha==previousGame&&(!library[n].metadata||library[n].metadata->runnable)){launchLibrary(n);break;}
        if(argv)LocalFree(argv);
        if(!audio.start())toast(L"Звуковое устройство недоступно. Эмуляция продолжает работать.");
        pads.init();timeBeginPeriod(1);
        HANDLE timer=CreateWaitableTimerExW(nullptr,nullptr,2,TIMER_ALL_ACCESS);
        if(!timer)timer=CreateWaitableTimerW(nullptr,FALSE,nullptr);
        auto previous=Clock::now(),lastStatus=previous,lastPad=previous,lastMetrics=previous;
        std::uint64_t processLast=0;double rewindAccumulator=0;
        auto processTime=[](){FILETIME creation{},exit{},kernel{},user{};if(!GetProcessTimes(GetCurrentProcess(),&creation,&exit,&kernel,&user))return std::uint64_t{0};
            ULARGE_INTEGER k{},u{};k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;return static_cast<std::uint64_t>(k.QuadPart+u.QuadPart);};
        processLast=processTime();SYSTEM_INFO system{};GetSystemInfo(&system);
        while(!closing) {
            MSG message{};
            for(int n=0;n<64&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE);++n) {
                if(message.message==WM_QUIT) { closing=true;break; }
                if(message.message==WM_KEYDOWN&&message.hwnd==searchEdit){if(message.wParam==VK_ESCAPE){showCatalog(false);continue;}
                    if(message.wParam==VK_RETURN){if(!visibleGames.empty())launchLibrary(visibleGames.front());continue;}
                    if((GetKeyState(VK_CONTROL)&0x8000)&&(message.wParam=='O'||message.wParam=='L')){command(message.wParam=='O'?Open:Library);continue;}}
                TranslateMessage(&message);DispatchMessageW(&message);
            }
            if(closing)break;
            const auto now=Clock::now();
            double elapsed=std::chrono::duration<double>(now-previous).count();previous=now;
            elapsed=std::clamp(elapsed,0.0,0.05);
            if(clockReset){elapsed=0;hostAccumulator=0;rewindAccumulator=0;clockReset=false;}
            if(now-lastPad>=std::chrono::milliseconds(4)) {updateKeys();lastPad=now;}
            const bool wasHalted=cpu.halted();
            if(rewinding&&!timelineDragging){rewindAccumulator+=elapsed*120;const int steps=static_cast<int>(rewindAccumulator);rewindAccumulator-=steps;
                if(steps>0&&rewindCursor)restoreTimeline(rewindCursor>static_cast<std::size_t>(steps)?rewindCursor-steps:0);}
            bool advanced=false;
            if(running()){
                const bool fast=turboMode||physicalKeys[0x0F],slow=slowMode||physicalKeys[0x1D]||physicalKeys[0x9D];
                hostAccumulator+=elapsed*(fast?4.0:slow?0.25:1.0);
                int frames=0;while(hostAccumulator+1e-12>=1.0/60&&running()&&frames++<16){hostAccumulator-=1.0/60;advanceVmFrame();advanced=true;}
            }
            if(advanced)redraw();
            if(cpu.halted()&&!wasHalted) {audio.silence();refresh();}
            audio.pump(running()&&!muted);
            if(now-lastStatus>=std::chrono::milliseconds(250)) {redraw();lastStatus=now;}
            if(now-lastMetrics>=std::chrono::seconds(1)) {
                const double seconds=std::chrono::duration<double>(now-lastMetrics).count();const auto current=processTime();
                measuredCpu=std::clamp((current-processLast)/10000000.0/seconds*100/std::max<DWORD>(1,system.dwNumberOfProcessors),0.0,100.0);
                measuredFps=metricFrames/seconds;const auto count=cpu.instructionCount();measuredIps=count>=metricInstructions?(count-metricInstructions)/seconds:0;
                metricInstructions=count;metricFrames=0;processLast=current;lastMetrics=now;
            }
            // Batch inexpensive CPU cycles; wake for input/audio or the next
            // display deadline, rather than once per emulated instruction.
            const double wait=running()?std::clamp(1.0/60-hostAccumulator,0.0002,0.002):0.004;
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
        if(smokeMode){std::ofstream report(dataDir/L"smoke-result.txt");report<<"FAIL\nException: "<<problem.what()<<'\n';stopMedia();covers.clear();if(gdiplusToken)Gdiplus::GdiplusShutdown(gdiplusToken);return 2;}
        MessageBoxW(window,wide(problem.what()).c_str(),L"CHIP-8 Studio — ошибка",MB_OK|MB_ICONERROR);
        stopMedia();audio.stop();covers.clear();if(gdiplusToken)Gdiplus::GdiplusShutdown(gdiplusToken);return 1;
    }
}
