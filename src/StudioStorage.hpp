#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace chip8_storage {
struct Entry {std::string name;std::vector<std::uint8_t> bytes;};
inline std::uint32_t checksum(const std::uint8_t* bytes,std::size_t size) {
    std::uint32_t value=~0u;for(std::size_t n=0;n<size;++n){value^=bytes[n];for(int bit=0;bit<8;++bit)value=(value>>1)^(0xEDB88320u&(0u-(value&1)));}return ~value;
}
inline bool safeName(const std::string& name) {
    auto isHex=[](const std::string& text,std::size_t n){return text.size()==n&&std::all_of(text.begin(),text.end(),[](unsigned char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');});};
    if(name=="settings.ini")return true;
    if(name.starts_with("roms/")&&name.ends_with(".ch8"))return isHex(name.substr(5,name.size()-9),40);
    if(name.size()<20||!isHex(name.substr(0,16),16))return false;
    const auto suffix=name.substr(16);
    if(suffix==".rpl"||suffix==".c8state"||suffix==".auto.c8state")return true;
    return suffix.size()==14&&suffix.starts_with(".slot")&&suffix[5]>='0'&&suffix[5]<='9'&&suffix.substr(6)==".c8state";
}
inline std::vector<std::uint8_t> encode(const std::vector<Entry>& entries) {
    if(entries.size()>4096)return {};
    std::vector<std::uint8_t> out{'C','8','P','R','E','F','S',1};
    auto put=[&](std::uint64_t value,unsigned size){for(unsigned n=0;n<size;++n)out.push_back(static_cast<std::uint8_t>(value>>(8*n)));};put(entries.size(),4);
    std::vector<std::string> names;
    for(const auto& entry:entries) {
        if(!safeName(entry.name)||entry.bytes.size()>4u*1024u*1024u||std::find(names.begin(),names.end(),entry.name)!=names.end())return {};
        names.push_back(entry.name);put(entry.name.size(),2);put(entry.bytes.size(),4);
        out.insert(out.end(),entry.name.begin(),entry.name.end());out.insert(out.end(),entry.bytes.begin(),entry.bytes.end());
        if(out.size()>64u*1024u*1024u-4)return {};
    }
    put(checksum(out.data(),out.size()),4);return out;
}
inline bool decode(const std::vector<std::uint8_t>& bytes,std::vector<Entry>& entries) {
    const std::vector<std::uint8_t> magic{'C','8','P','R','E','F','S',1};
    if(bytes.size()<16||bytes.size()>64u*1024u*1024u||!std::equal(magic.begin(),magic.end(),bytes.begin()))return false;
    std::size_t pos=8;const auto end=bytes.size()-4;
    auto get=[&](unsigned size){std::uint64_t v=0;for(unsigned n=0;n<size;++n)v|=static_cast<std::uint64_t>(bytes[pos++])<<(8*n);return v;};
    const auto count=get(4);if(count>4096)return false;
    std::vector<Entry> result;
    for(std::uint64_t n=0;n<count;++n) {
        if(pos+6>end)return false;
        const auto nameSize=get(2),size=get(4);
        if(nameSize>100||size>4u*1024u*1024u||nameSize+size>end-pos)return false;
        Entry entry;entry.name.assign(bytes.begin()+pos,bytes.begin()+pos+nameSize);pos+=nameSize;
        if(!safeName(entry.name)||std::any_of(result.begin(),result.end(),[&](const auto& old){return old.name==entry.name;}))return false;
        entry.bytes.assign(bytes.begin()+pos,bytes.begin()+pos+size);pos+=size;result.push_back(std::move(entry));
    }
    if(pos!=end||get(4)!=checksum(bytes.data(),end))return false;
    entries=std::move(result);return true;
}
} // namespace chip8_storage
