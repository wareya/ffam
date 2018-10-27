#include <stdio.h>
#include <vector>
#include <string>
#include <map>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "deps/stb_image.h"

typedef uint16_t u16;
typedef uint32_t u32;
typedef int16_t s16;
typedef int32_t s32;

// for write

struct chix
{
    u32 codepoint;
    char flags = 0; // set to 0
    u32 addr;
};

struct chardef
{
    u16 width;  // pixels bbox
    u16 height; // pixels bbox
    s16 xoffset; // relative to former device advancement
    s16 yoffset; // upwards relative to baseline? (0 for '.', 4 for ',', 0 for 'A')
    s16 devicewidth; // Let it be known that devicewidth can be used to make the next character overlap the former
    std::vector<char> pixels;
};

// for read

struct chardat
{
    u32 codepoint=0;
    s32 right=0;
    s32 extra=0;
};

// for read/write

struct PFF2
{
    std::string NAME="Untitled Regular 12";
    std::string FAMI="Untitled";
    std::string WEIG="normal";
    std::string SLAN="normal";
    u16 PTSZ = 12;
    u16 MAXW = 12;
    u16 MAXH = 12;
    u16 ASCE = 12;
    u16 DESC = 3;
    std::vector<chardat> chars;
    std::vector<chix> CHIX;
};

char * read_line(FILE * f)
{
    auto start = ftell(f);
    unsigned i = 0;
    int c = 0;
    while(i++,c=fgetc(f),c!='\n' and c!=EOF);
    auto len = ftell(f)-start;
    if(ferror(f) or feof(f)) return nullptr;
    fseek(f,start,SEEK_SET);
    char * line = (char*)malloc(len+1);
    if(!line) return nullptr;
    if(fgets(line,len+1,f) != line) { free(line); return nullptr; }
    return line;
}

struct BILE
{
    char* buf = nullptr;
    size_t len = 0;
    size_t cap = 0;
    size_t off = 0;
};

// reallocate if necessary
int b_maybe_realloc(BILE* stream, size_t newsize)
{
    // input size is in target size/length, not target capacity
    if(newsize > stream->cap or newsize*2 < stream->cap)
    {
        ssize_t boost = stream->cap*0.5;
        if(boost > 4096)
            boost = 4096;
        if(newsize*2 < stream->cap)
            boost = -boost/2;
        size_t newcap = stream->cap+boost;
        if(newcap < 64)
            newcap = 64;
        char * newbuf = (char*)malloc(newcap);
        if(!newbuf)
            return 1;
        
        if(stream->buf)
        {
            memcpy(newbuf,stream->buf,stream->cap);
            memset(newbuf+stream->cap, 0, newcap-stream->cap);
            free(stream->buf);
        }
        else
        {
            memset(newbuf, 0, newcap);
        }
        stream->buf = newbuf;
        stream->cap = newcap;
        stream->len = newsize;
    }
    stream->len = newsize;
    return 0;
}

BILE* bopen()
{
    auto my = new BILE;
    return my;
}
void bclose(BILE* stream)
{
    if(stream)
        free(stream->buf);
    free(stream);
}

size_t bread(BILE* stream, size_t size, size_t count, char* buffer)
{
    if(stream->len < size*count)
        return 0;
    size_t i = 0;
    for(;i < size*count and i < stream->len; i += size)
        memcpy(&buffer[i], &stream->buf[stream->off+i], size);
    stream->off += i;
    return i/size;
}
// if buffer is null, zeroes are used
size_t bwrite(BILE* stream, size_t size, size_t count, const char* buffer)
{
    if(b_maybe_realloc(stream, stream->off+size*count))
        return 0;
    size_t i = 0;
    for(;i < size*count and i < stream->len; i += size)
    {
        if(buffer)
            memcpy(&stream->buf[stream->off+i], &buffer[i], size);
        else
            memset(&stream->buf[stream->off+i],0,size);
    }
    stream->off += i;
    return i/size;
}
template<typename T>
size_t bwrite(BILE* stream, T thing)
{
    size_t size = sizeof(T);
    if(b_maybe_realloc(stream, stream->off+size))
        return 0;
    const char* buffer = (char*)&thing;
    memcpy(&stream->buf[stream->off], &buffer[0], size);
    stream->off += size;
    return 1;
}
// delete N *bytes* out from under the current position
size_t bclear(BILE* stream, size_t size); 

#include <stdio.h>
ssize_t bseek(BILE* stream, ssize_t offset, int origin )
{
    if(!stream)
        return 0;
    
    size_t startpos = stream->off;
    
    if(origin == SEEK_SET)
        stream->off = offset;
    if(origin == SEEK_CUR)
        stream->off += offset;
    if(origin == SEEK_END)
        stream->off = stream->len+offset;
    
    if(stream->off > stream->len)
        stream->off = stream->len;
    if((ssize_t)stream->off < 0)
        stream->off = 0;
    
    return stream->off-startpos;
}
size_t btell(BILE* stream)
{
    if(!stream)
        return 0;
    return stream->off;
}
size_t bsize(BILE* stream)
{
    if(!stream)
        return 0;
    return stream->len;
}

// Returns a pointer to a block of bytes in the stream. This pointer CAN AND WILL invalidate if you mutate the stream in the meantime. DO NOT STORE IT.
// Returns null if the requested block is not entirely present in the data already or if the stream is invalid.
// if offset is -1, uses the stream offset.
char* bdata(BILE* stream, size_t offset, size_t count)
{
    if(!stream)
        return nullptr;
    if(offset == size_t(~0))
        offset = stream->off;
    if(offset+count > stream->len)
        return nullptr;
    return stream->buf+offset;
}

u32 byteswap_32(u32 in)
{
    return ((in & 0xFF) << 24)
         | ((in & 0xFF00) << 8)
         | ((in >> 8) & 0xFF00)
         |  (in >> 24);
}
u16 byteswap_16(u16 in)
{
    return ((in & 0xFF) << 8) | (in >> 8);
}

void string_macro(BILE* stream, const std::string& str)
{
    bwrite(stream, byteswap_32(str.size()+1));
    bwrite(stream, 1, str.size()+1, str.data());
}

struct img
{
    int w,h,n;
    unsigned char * data;
    img(const char * filename)
    {
        data = stbi_load(filename, &w, &h, &n, 4);
    }
    img(int w, int h, u32 color)
    {
        this->w = w;
        this->h = h;
        this->n = 4;
        this->data = (unsigned char *)malloc(w*h*4);
        auto data2 = (u32*)data;
        
        for(size_t i = 0; i < w*h; i++)
            data2[i] = color;
    }
    ~img()
    {
        free(data);
    }
};

int main(int argc, char ** argv)
{
    if(argc < 3)
    {
        puts("Usage: program inname out.pf2");
        return 0;
    }
    
    std::string intxt(argv[1]);
    intxt += ".txt";
    std::string inimg(argv[1]);
    inimg += ".bmp";
    
    FILE * f = fopen(intxt.data(),"r");
    if(!f)
    {
        puts("Couldn't open definition.");
        return 0;
    }
    
    img myimage(inimg.data());
    if(!myimage.data)
    {
        puts("Couldn't open image.");
        return 0;
    }
    
    PFF2 myfont;
    std::vector<chardat> chars;
    
    while(!ferror(f) and !feof(f))
    {
        char * line = read_line(f);
        if(line == nullptr) break;
        char key[256];
        char kind[2] = " ";
        char value[256];
        
        toplevel:
        auto count = sscanf(line, "%255[^=:]%1[=:]%255[^\n]", key, kind, value);
        
        if(count == EOF) break;
        if(kind[0] == '=' and count == 3)
        {
            if(strncmp(key, "NAME", 255) == 0)
            {
                myfont.NAME=value;
                printf("Name: %s\n",myfont.NAME.data());
            } else
            if(strncmp(key, "FAMI", 255) == 0)
            {
                myfont.FAMI=value;
                printf("Family: %s\n",myfont.FAMI.data());
            } else
            if(strncmp(key, "WEIG", 255) == 0)
            {
                myfont.WEIG=value;
                printf("Weight: %s\n",myfont.WEIG.data());
            } else
            if(strncmp(key, "SLAN", 255) == 0)
            {
                myfont.SLAN=value;
                printf("Slant: %s\n",myfont.SLAN.data());
            } else
            if(strncmp(key, "PTSZ", 255) == 0)
            {
                sscanf(value, "%hu", &myfont.PTSZ);
                printf("Points: %hu\n",myfont.PTSZ);
            } else
            if(strncmp(key, "MAXH", 255) == 0)
            {
                sscanf(value, "%hu", &myfont.MAXH);
                printf("Max height: %hu\n",myfont.MAXH);
            } else
            if(strncmp(key, "MAXW", 255) == 0)
            {
                sscanf(value, "%hu", &myfont.MAXW);
                printf("Max width: %hu\n",myfont.MAXW);
            } else
            if(strncmp(key, "ASCE", 255) == 0)
            {
                sscanf(value, "%hu", &myfont.ASCE);
                printf("Ascent: %hu\n",myfont.ASCE);
            } else
            if(strncmp(key, "DESC", 255) == 0)
            {
                sscanf(value, "%hu", &myfont.DESC);
                printf("Descent: %hu\n",myfont.DESC);
            }
        }
        if(kind[0] == ':' and count == 2)
        {
            if(strncmp(key, "CHAR", 255) == 0)
            {
                chardat mydat;
                while(!ferror(f) and !feof(f))
                {
                    line = read_line(f);
                    if(line == nullptr) {chars.push_back(mydat); goto fallthru;}
                    if(line[0] == '\0') break;
                    if(line[0] == '\n') continue; // blank line
                    if(line[0] != ' ' and line[0] != '\t') break;
                    
                    auto count = sscanf(line, "%*[ \t]%255[^=:]%1[=:]%255[^\n]", key, kind, value);
                    if(count < 0) {puts("reeee");continue;} // string invalid or something
                    if(kind[0] == '=' and count == 3)
                    {
                        if(strncmp(key, "CODEPOINT", 255) == 0)
                        {
                            sscanf(value, "U%X", &mydat.codepoint);
                        } else
                        if(strncmp(key, "RIGHT", 255) == 0)
                        {
                            sscanf(value, "%d", &mydat.right);
                        } else
                        if(strncmp(key, "EXTRA", 255) == 0)
                        {
                            sscanf(value, "%d", &mydat.extra);
                        }
                    }
                }
                chars.push_back(mydat);
                goto toplevel;
            }
        }
    }
    fallthru:
    
    #define c_bg 0xFF606060
    #define c_fg 0xFFFFFFFF
    #define c_red 0xFF0000FF
    #define c_null 0xFF000000
    
    auto w = myimage.w;
    auto h = myfont.ASCE + myfont.DESC;
    
    auto x = 0;
    auto y = 0;
    u32 * pixels = (u32*)myimage.data;
    
    printf("Chars: %lu\n",chars.size());
    
    std::vector<chardef> defs;
    std::vector<chix> chixes;
    
    for(auto c : chars)
    {
        auto d = 0;
        while(x+d<w and pixels[x+d]!=c_null) d++;
        auto minx = d;
        auto maxx = 0;
        auto miny = h;
        auto maxy = 0;
        for(auto i = 0; i < d*h; i++)
        {
            auto rx = i%d;
            auto ry = i/d;
            if(pixels[x+rx + w*ry] != c_bg)
            {
                if(rx < minx) minx = rx;
                if(rx > maxx) maxx = rx;
                if(ry < miny) miny = ry;
                if(ry > maxy) maxy = ry;
            }
        }
        u32 width;
        u32 height;
        if(maxy < miny)
        {
            width = 0;
            height = 0;
        }
        else
        {
            width = maxx-minx+1;
            height =  maxy-miny+1;
        }
        std::vector<char> bits;
        for(auto ry = miny; ry <= maxy; ry++)
            for(auto rx = minx; rx <= maxx; rx++)
                bits.push_back(pixels[x+rx + w*ry] == c_fg);
        
        chardef mydef;
        chix mychix;
        mydef.width = width;
        mydef.height = height;
        mydef.xoffset = c.right;
        mydef.yoffset = myfont.ASCE-maxy-1;
        mydef.devicewidth = mydef.width+c.extra;
        mydef.pixels = bits;
        mychix.codepoint = c.codepoint;
        mychix.addr = 0;
        chixes.push_back(mychix);
        defs.push_back(mydef);
        x += d+1;
    }
    
    BILE * basedata = bopen();
    
    bwrite(basedata, 4, 1, "FILE");
    bwrite(basedata, byteswap_32(4));
    bwrite(basedata, 4, 1, "PFF2");
    
    bwrite(basedata, 4, 1, "NAME");
    string_macro(basedata, myfont.NAME);
    bwrite(basedata, 4, 1, "FAMI");
    string_macro(basedata, myfont.FAMI);
    bwrite(basedata, 4, 1, "WEIG");
    string_macro(basedata, myfont.WEIG);
    bwrite(basedata, 4, 1, "SLAN");
    string_macro(basedata, myfont.SLAN);
    
    bwrite(basedata, 4, 1, "PTSZ");
    bwrite(basedata, byteswap_32(2));
    bwrite(basedata, byteswap_16(myfont.PTSZ));
    bwrite(basedata, 4, 1, "MAXW");
    bwrite(basedata, byteswap_32(2));
    bwrite(basedata, byteswap_16(myfont.MAXW));
    bwrite(basedata, 4, 1, "MAXH");
    bwrite(basedata, byteswap_32(2));
    bwrite(basedata, byteswap_16(myfont.MAXH));
    bwrite(basedata, 4, 1, "ASCE");
    bwrite(basedata, byteswap_32(2));
    bwrite(basedata, byteswap_16(myfont.ASCE));
    bwrite(basedata, 4, 1, "DESC");
    bwrite(basedata, byteswap_32(2));
    bwrite(basedata, byteswap_16(myfont.DESC));
    
    auto CHIX_addr = btell(basedata);
    bwrite(basedata, 4, 1, "CHIX");
    bwrite(basedata, byteswap_32(chixes.size()*9));
    for(auto c : chixes)
    {
        bwrite(basedata, byteswap_32(c.codepoint));
        bwrite(basedata, char(0));
        bwrite(basedata, u32(0)); // to be overwritten by later code
    }
    
    bwrite(basedata, 4, 1, "DATA");
    bwrite(basedata, u32(0xFFFFFFFF));
    std::vector<u32> offsets;
    for(auto d : defs)
    {
        offsets.push_back(btell(basedata));
        bwrite(basedata, byteswap_16(d.width));
        bwrite(basedata, byteswap_16(d.height));
        bwrite(basedata, byteswap_16(d.xoffset));
        bwrite(basedata, byteswap_16(d.yoffset));
        bwrite(basedata, byteswap_16(d.devicewidth));
        auto bytes = (d.width*d.height + 7)/8;
        int counter = 0;
        char c = 0;
        for(auto b : d.pixels)
        {
            c |= (b!=0) << (7-counter);
            counter++;
            if(counter == 8)
            {
                bwrite(basedata, c);
                c = 0;
                counter = 0;
            }
        }
        if(counter != 0) // did not flush the last bits mixed
            bwrite(basedata, c);
    }
    
    bseek(basedata, CHIX_addr+4+4+4+1, SEEK_SET);
    
    for(auto o : offsets)
    {
        *(u32*)bdata(basedata, -1, 4) = byteswap_32(o);
        bseek(basedata, 9, SEEK_CUR);
    }
    
    auto awfe = fopen(argv[2],"wb");
    int err = fwrite(bdata(basedata, 0, bsize(basedata)), 1, bsize(basedata), awfe);
    
    return 0;
}
