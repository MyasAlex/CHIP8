#include "StudioMedia.hpp"
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    chip8_media::GifWriter writer;if(!writer.start(argv[1]))return 3;
    std::vector<std::uint32_t> pixels(256*128);
    for(int frame=0;frame<12;++frame) {
        for(int y=0;y<128;++y)for(int x=0;x<256;++x) {
            const auto r=(x*7+y*11+frame*13)&255,g=(x*19+y*3+frame*31)&255,b=(x*23+y*29+frame*7)&255;
            pixels[y*256+x]=(r<<16)|(g<<8)|b;
        }
        if(!writer.frame(pixels))return 4;
    }
    if(!writer.finish())return 5;std::cout<<"GIF fixture: 12 frames\n";return 0;
}
