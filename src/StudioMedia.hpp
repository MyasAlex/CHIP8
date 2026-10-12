#pragma once
// Streaming media writers. GIF works without any external dependency; Windows
// MP4 loads the installed Media Foundation APIs only when recording is requested.
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <vector>

namespace chip8_media {
class GifWriter {
public:
    bool start(const std::filesystem::path& path,int width=256,int height=128) {
        finish();if(width<1||height<1||width>1024||height>1024)return false;
        width_=width;height_=height;frames_=0;file_.open(path,std::ios::binary|std::ios::trunc);
        if(!file_)return false;
        file_.write("GIF89a",6);word(width);word(height);byte(0xF7);byte(0);byte(0);
        for(int n=0;n<256;++n) {byte(((n>>5)&7)*255/7);byte(((n>>2)&7)*255/7);byte((n&3)*255/3);}
        const std::array<unsigned char,19> loop{0x21,0xFF,11,'N','E','T','S','C','A','P','E','2','.','0',3,1,0,0,0};
        file_.write(reinterpret_cast<const char*>(loop.data()),loop.size());return file_.good();
    }
    bool frame(const std::vector<std::uint32_t>& pixels) {
        if(!active()||pixels.size()!=static_cast<std::size_t>(width_)*height_)return false;
        // Centisecond durations 3,3,4 sum to exactly 100 ms for three frames.
        byte(0x21);byte(0xF9);byte(4);byte(4);word(frames_%3==2?4:3);byte(0);byte(0);
        byte(0x2C);word(0);word(0);word(width_);word(height_);byte(0);byte(8);
        std::vector<std::uint8_t> packed;packed.reserve(pixels.size()/3);
        unsigned bits=9,next=258,accumulator=0,used=0;
        auto emit=[&](unsigned code) {
            accumulator|=code<<used;used+=bits;
            while(used>=8) {packed.push_back(static_cast<std::uint8_t>(accumulator));accumulator>>=8;used-=8;}
        };
        auto quantize=[](std::uint32_t rgb){return ((rgb>>16)&0xE0)|((rgb>>11)&0x1C)|((rgb>>6)&3);};
        std::unordered_map<unsigned,unsigned> dictionary;dictionary.reserve(4096);
        emit(256);unsigned prefix=quantize(pixels.front());
        for(std::size_t index=1;index<pixels.size();++index) {
            const unsigned symbol=quantize(pixels[index]),key=(prefix<<8)|symbol;
            const auto found=dictionary.find(key);
            if(found!=dictionary.end()) {prefix=found->second;continue;}
            emit(prefix);
            if(next<4096) {
                dictionary.emplace(key,next++);
                // The decoder adds one entry later, after receiving the next code.
                if(next>(1u<<bits)&&bits<12)++bits;
            } else {emit(256);dictionary.clear();bits=9;next=258;}
            prefix=symbol;
        }
        emit(prefix);
        if(next==(1u<<bits)&&bits<12)++bits;
        emit(257);if(used)packed.push_back(static_cast<std::uint8_t>(accumulator));
        for(std::size_t index=0;index<packed.size();index+=255) {
            const auto count=std::min<std::size_t>(255,packed.size()-index);byte(static_cast<int>(count));
            file_.write(reinterpret_cast<const char*>(packed.data()+index),static_cast<std::streamsize>(count));
        }
        byte(0);++frames_;return file_.good()&&static_cast<std::uint64_t>(file_.tellp())<128u*1024u*1024u;
    }
    bool finish() {if(!file_.is_open())return true;byte(0x3B);file_.flush();const bool ok=file_.good();file_.close();return ok;}
    bool active()const{return file_.is_open();}
    unsigned frames()const{return frames_;}
    ~GifWriter(){finish();}
private:
    void byte(int value){file_.put(static_cast<char>(value));}
    void word(int value){byte(value&255);byte((value>>8)&255);}
    std::ofstream file_;int width_=256,height_=128;unsigned frames_=0;
};
} // namespace chip8_media

#ifdef _WIN32
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#ifdef _MSC_VER
#pragma comment(lib,"Mfuuid.lib")
#endif
namespace chip8_media {
template<class T> struct Com {
    T* p=nullptr;
    ~Com(){reset();}void reset(){if(p)p->Release();p=nullptr;}
    T** put(){reset();return &p;}T* operator->()const{return p;}
};
class Mp4Writer {
public:
    bool start(const std::filesystem::path& path) {
        finish();error_=S_OK;frames_=0;
        platform_=LoadLibraryExW(L"mfplat.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        reader_=LoadLibraryExW(L"mfreadwrite.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!platform_||!reader_)return fail(HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND));
        startup_=resolve<Startup>(platform_,"MFStartup");shutdown_=resolve<Shutdown>(platform_,"MFShutdown");
        type_=resolve<Type>(platform_,"MFCreateMediaType");sample_=resolve<Sample>(platform_,"MFCreateSample");
        buffer_=resolve<Buffer>(platform_,"MFCreateMemoryBuffer");sink_=resolve<Sink>(reader_,"MFCreateSinkWriterFromURL");
        if(!startup_||!shutdown_||!type_||!sample_||!buffer_||!sink_)return fail(E_NOTIMPL);
        const HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);com_=SUCCEEDED(com);
        if(FAILED(com)&&com!=RPC_E_CHANGED_MODE)return fail(com);
        HRESULT hr=startup_(MF_VERSION,MFSTARTUP_FULL);if(FAILED(hr))return fail(hr);started_=true;
        hr=sink_(path.c_str(),nullptr,nullptr,writer_.put());if(FAILED(hr))return fail(hr);
        Com<IMFMediaType> output,input;
        hr=type_(output.put());if(FAILED(hr))return fail(hr);
        auto video=[&](IMFMediaType* t,const GUID& subtype) {
            HRESULT h=t->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video);
            if(SUCCEEDED(h))h=t->SetGUID(MF_MT_SUBTYPE,subtype);
            if(SUCCEEDED(h))h=t->SetUINT32(MF_MT_INTERLACE_MODE,MFVideoInterlace_Progressive);
            if(SUCCEEDED(h))h=t->SetUINT64(MF_MT_FRAME_SIZE,(512ull<<32)|256);
            if(SUCCEEDED(h))h=t->SetUINT64(MF_MT_FRAME_RATE,(30ull<<32)|1);
            if(SUCCEEDED(h))h=t->SetUINT64(MF_MT_PIXEL_ASPECT_RATIO,(1ull<<32)|1);
            return h;
        };
        hr=video(output.p,MFVideoFormat_H264);
        if(SUCCEEDED(hr))hr=output->SetUINT32(MF_MT_AVG_BITRATE,1500000);
        if(SUCCEEDED(hr))hr=writer_->AddStream(output.p,&video_);
        if(SUCCEEDED(hr))hr=type_(input.put());
        if(SUCCEEDED(hr))hr=video(input.p,MFVideoFormat_RGB32);
        if(SUCCEEDED(hr))hr=input->SetUINT32(MF_MT_DEFAULT_STRIDE,512*4);
        if(SUCCEEDED(hr))hr=writer_->SetInputMediaType(video_,input.p,nullptr);
        if(FAILED(hr))return fail(hr);
        output.reset();input.reset();hr=type_(output.put());
        auto audio=[&](IMFMediaType* t,const GUID& subtype) {
            HRESULT h=t->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Audio);
            if(SUCCEEDED(h))h=t->SetGUID(MF_MT_SUBTYPE,subtype);
            if(SUCCEEDED(h))h=t->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS,1);
            if(SUCCEEDED(h))h=t->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,48000);
            if(SUCCEEDED(h))h=t->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE,16);
            return h;
        };
        if(SUCCEEDED(hr))hr=audio(output.p,MFAudioFormat_AAC);
        if(SUCCEEDED(hr))hr=output->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND,12000);
        if(SUCCEEDED(hr))hr=output->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE,0);
        if(SUCCEEDED(hr))hr=output->SetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION,0x29);
        if(SUCCEEDED(hr))hr=writer_->AddStream(output.p,&audio_);
        if(SUCCEEDED(hr))hr=type_(input.put());
        if(SUCCEEDED(hr))hr=audio(input.p,MFAudioFormat_PCM);
        if(SUCCEEDED(hr))hr=input->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT,2);
        if(SUCCEEDED(hr))hr=input->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND,96000);
        if(SUCCEEDED(hr))hr=writer_->SetInputMediaType(audio_,input.p,nullptr);
        if(SUCCEEDED(hr))hr=writer_->BeginWriting();
        if(FAILED(hr))return fail(hr);
        frames_=0;active_=true;return true;
    }
    bool frame(const std::vector<std::uint32_t>& pixels,const std::array<std::int16_t,1600>& sound) {
        if(!active_||pixels.size()!=512u*256u)return false;
        std::vector<std::uint32_t> bottomUp(pixels.size());
        for(int y=0;y<256;++y)std::copy_n(pixels.data()+y*512,512,bottomUp.data()+(255-y)*512);
        const LONGLONG time=static_cast<LONGLONG>(frames_)*10000000/30;
        const LONGLONG duration=static_cast<LONGLONG>(frames_+1)*10000000/30-time;
        HRESULT hr=write(video_,bottomUp.data(),static_cast<DWORD>(bottomUp.size()*4),time,duration);
        if(SUCCEEDED(hr))hr=write(audio_,sound.data(),static_cast<DWORD>(sound.size()*2),time,duration);
        if(FAILED(hr)){error_=hr;return false;}++frames_;return true;
    }
    bool finish() {
        HRESULT hr=S_OK;if(writer_.p&&active_)hr=writer_->Finalize();active_=false;writer_.reset();
        if(started_&&shutdown_)shutdown_();
        started_=false;
        if(com_)CoUninitialize();
        com_=false;
        if(reader_)FreeLibrary(reader_);
        if(platform_)FreeLibrary(platform_);
        reader_=platform_=nullptr;
        if(FAILED(hr))error_=hr;
        return SUCCEEDED(hr);
    }
    bool active()const{return active_;}unsigned frames()const{return frames_;}HRESULT error()const{return error_;}
    ~Mp4Writer(){finish();}
private:
    template<class T>static T resolve(HMODULE m,const char* name) {auto a=GetProcAddress(m,name);T p=nullptr;std::memcpy(&p,&a,sizeof(p));return p;}
    bool fail(HRESULT hr){error_=hr;return false;}
    HRESULT write(DWORD stream,const void* data,DWORD size,LONGLONG time,LONGLONG duration) {
        Com<IMFSample> sample;Com<IMFMediaBuffer> buffer;
        HRESULT hr=sample_(sample.put());if(SUCCEEDED(hr))hr=buffer_(size,buffer.put());
        BYTE* target=nullptr;if(SUCCEEDED(hr))hr=buffer->Lock(&target,nullptr,nullptr);
        if(SUCCEEDED(hr)){std::memcpy(target,data,size);hr=buffer->Unlock();}
        if(SUCCEEDED(hr))hr=buffer->SetCurrentLength(size);
        if(SUCCEEDED(hr))hr=sample->AddBuffer(buffer.p);
        if(SUCCEEDED(hr))hr=sample->SetSampleTime(time);
        if(SUCCEEDED(hr))hr=sample->SetSampleDuration(duration);
        if(SUCCEEDED(hr))hr=writer_->WriteSample(stream,sample.p);
        return hr;
    }
    using Startup=HRESULT(WINAPI*)(ULONG,DWORD);using Shutdown=HRESULT(WINAPI*)();
    using Type=HRESULT(WINAPI*)(IMFMediaType**);using Sample=HRESULT(WINAPI*)(IMFSample**);
    using Buffer=HRESULT(WINAPI*)(DWORD,IMFMediaBuffer**);
    using Sink=HRESULT(WINAPI*)(LPCWSTR,IMFByteStream*,IMFAttributes*,IMFSinkWriter**);
    HMODULE platform_=nullptr,reader_=nullptr;bool started_=false,com_=false,active_=false;
    Startup startup_=nullptr;Shutdown shutdown_=nullptr;Type type_=nullptr;Sample sample_=nullptr;Buffer buffer_=nullptr;Sink sink_=nullptr;
    Com<IMFSinkWriter> writer_;DWORD video_=0,audio_=0;unsigned frames_=0;HRESULT error_=S_OK;
};
} // namespace chip8_media
#endif
