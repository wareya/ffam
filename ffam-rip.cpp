#include <stdio.h>
#include <vector>
#include <string>
#include <map>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "deps/stb_image_write.h"

typedef uint16_t u16;
typedef uint32_t u32;
typedef int16_t s16;
typedef int32_t s32;

struct chix
{
    u32 codepoint;
    char flags;
    u32 addr;
};

struct PFF2
{
    std::string NAME="";
    std::string FAMI;
    std::string WEIG;
    std::string SLAN;
    u16 PTSZ = ~0;
    u16 MAXW = ~0;
    u16 MAXH = ~0;
    u16 ASCE = ~0;
    u16 DESC = ~0;
    std::vector<chix> CHIX;
};


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

std::string get_string(FILE* f)
{
    u32 len;
    fread(&len,4,1,f);
    len = byteswap_32(len);
    if(len > 255) len = 255; // Abusive font.
    char * str = (char*)malloc(len+1);
    fread(str,1,len,f);
    str[len] = 0;
    std::string ret(str);
    free(str);
    return ret;
}

void loadNAME(PFF2* font, FILE* f)
{
    font->NAME = get_string(f);
    printf("Font name: %s\n", font->NAME.data());
}

void loadFAMI(PFF2* font, FILE* f)
{
    font->FAMI = get_string(f);
    printf("Font family: %s\n", font->FAMI.data());
}

void loadWEIG(PFF2* font, FILE* f)
{
    font->WEIG = get_string(f);
    printf("Font weight: %s\n", font->WEIG.data());
}

void loadSLAN(PFF2* font, FILE* f)
{
    font->SLAN = get_string(f);
    printf("Font slant: %s\n", font->SLAN.data());
}

void loadPTSZ(PFF2* font, FILE* f)
{
    fread(&font->PTSZ,2,1,f);
    font->PTSZ = byteswap_16(font->PTSZ);
    printf("Font pt size: %d\n",font->PTSZ);
}
void loadMAXW(PFF2* font, FILE* f)
{
    fread(&font->MAXW,2,1,f);
    font->MAXW = byteswap_16(font->MAXW);
    printf("Font max width: %d\n",font->MAXW);
}
void loadMAXH(PFF2* font, FILE* f)
{
    fread(&font->MAXH,2,1,f);
    font->MAXH = byteswap_16(font->MAXH);
    printf("Font max height: %d\n",font->MAXH);
}
void loadASCE(PFF2* font, FILE* f)
{
    fread(&font->ASCE,2,1,f);
    font->ASCE = byteswap_16(font->ASCE);
    printf("Font ascent: %d\n",font->ASCE);
}
void loadDESC(PFF2* font, FILE* f)
{
    fread(&font->DESC,2,1,f);
    font->DESC = byteswap_16(font->DESC);
    printf("Font descent: %d\n",font->DESC);
}
void nextCHIX(PFF2* font, FILE* f)
{
    chix mychix;
    fread(&mychix.codepoint,4,1,f);
    fread(&mychix.flags,1,1,f);
    fread(&mychix.addr,4,1,f);
    mychix.codepoint=byteswap_32(mychix.codepoint);
    mychix.addr=byteswap_32(mychix.addr);
    font->CHIX.push_back(mychix);
}
u32 count = 0;
void loadCHIX(PFF2* font, FILE* f)
{
    u32 size;
    fread(&size,4,1,f);
    size = byteswap_32(size);
    count = size/9;
    for(size_t i=0; i<count; i++)
        nextCHIX(font,f);
    printf("Char count: %u\n",count);
}

struct chardef
{
    u16 width;  // pixels bbox
    u16 height; // pixels bbox
    s16 xoffset; // relative to former device advancement
    s16 yoffset; // upwards relative to baseline? (0 for '.', 4 for ',', 0 for 'A')
    s16 devicewidth; // Let it be known that devicewidth can be used to make the next character overlap the former
    std::vector<char> pixels;
};

int usage(char *prog, int rc)
{
    printf("Usage: %s [ -c # ] <in.pf2> <outname>\n", prog);
    printf("   -c # - out image is character cells in # columns\n");
    exit(rc);
}

struct img
{
    int w,h,n;
    unsigned char * data;
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
    int i = 1;
    int display=0;

    if(0)
    {
        invalid:
        puts("Invalid pf2 file");
        return 0;
    }
    
    while(i < argc && argv[i][0] == '-')
    {
        if(argv[i][1]=='c')
        {
            i++;
            if(i < argc)
            {
                display=atoi(argv[i]);
                if(display < 0)
                {
                    usage(argv[0], 1);
                }
            }
        }
        else
        {
            printf("Unknown option: %s\n", argv[i]);
            exit(1);
        }
        i++;
    }

    if(argc - i < 2)
    {
        usage(argv[0], 1);
    }

    std::string outtxt(argv[i+1]);
    outtxt += ".txt";
    std::string outimg(argv[i+1]);
    outimg += ".bmp";
    
    FILE* f = fopen(argv[i],"rb");
    if(!f)
    {
        puts("Could not open pf2 file");
        return 0;
    }
    u32 data = 0;
    PFF2 myfont;
    
    fread(&data, 4, 1, f);
    if(data!=*(u32*)"FILE") goto invalid;
    
    fread(&data, 1, 4, f);
    if(byteswap_32(data)!=4) goto invalid;
    
    fread(&data, 4, 1, f);
    if(data!=*(u32*)"PFF2") goto invalid;
    
    #define check_invalid_simple_u16 {u32 junk; fread(&junk,4,1,f); if(byteswap_32(junk)!=2) goto invalid;}
    
    while(fread(&data, 4, 1, f),data!=*(u32*)"DATA" and !feof(f) and !ferror(f))
    {
        if(data==*(u32*)"NAME")
            loadNAME(&myfont, f);
        if(data==*(u32*)"FAMI")
            loadFAMI(&myfont, f);
        if(data==*(u32*)"WEIG")
            loadWEIG(&myfont, f);
        if(data==*(u32*)"SLAN")
            loadSLAN(&myfont, f);
        if(data==*(u32*)"PTSZ")
        {
            check_invalid_simple_u16;
            loadPTSZ(&myfont, f);
        }
        if(data==*(u32*)"MAXW")
        {
            check_invalid_simple_u16;
            loadMAXW(&myfont, f);
        }
        if(data==*(u32*)"MAXH")
        {
            check_invalid_simple_u16;
            loadMAXH(&myfont, f);
        }
        if(data==*(u32*)"ASCE")
        {
            check_invalid_simple_u16;
            loadASCE(&myfont, f);
        }
        if(data==*(u32*)"DESC")
        {
            check_invalid_simple_u16;
            loadDESC(&myfont, f);
        }
        if(data==*(u32*)"CHIX")
        {
            loadCHIX(&myfont, f);
        }
    }
    fread(&data, 4, 1, f);
    if(data!=0xFFFFFFFF) goto invalid;
    
    auto start = ftell(f);
    printf("Start of DATA: %lX\n",ftell(f));
    
    std::map<u32,chardef> definitions;
    
    u32 printnum = 16;
    u32 glyphs = 0;
    while(!ferror(f) and !feof(f))
    {
        u32 setpixels = 0;
        u32 addr = ftell(f);
        chardef mydef;
        fread(&mydef.width,2,1,f);
        fread(&mydef.height,2,1,f);
        fread(&mydef.xoffset,2,1,f);
        fread(&mydef.yoffset,2,1,f);
        fread(&mydef.devicewidth,2,1,f);
        mydef.width = byteswap_16(mydef.width);
        mydef.height = byteswap_16(mydef.height);
        mydef.xoffset = byteswap_16(mydef.xoffset);
        mydef.yoffset = byteswap_16(mydef.yoffset);
        mydef.devicewidth = byteswap_16(mydef.devicewidth);
        size_t length = (mydef.width*mydef.height+7)/8;
        char * data = nullptr;
        
        // check for read errors before malloc to avoid a malloc on junk length data
        if(ferror(f) or feof(f)) goto cont;
        
        data = (char*)malloc(length);
        
        if(data == nullptr) goto cont; // REEEEE
        
        fread(data,1,length,f);
        
        // check again now that we're done reading
        if(ferror(f) or feof(f)) goto cont;
        
        for(unsigned i=0;i<length;i++)
        {
            for(auto j=0;j<8;j++)
            {
                if(i*8+j >= mydef.width*mydef.height)
                    break;
                auto b = 128>>j;
                setpixels += (data[i]&b)!=0;
                mydef.pixels.push_back((data[i]&b)!=0);
            }
        }
        if(printnum)
        {
            printf("Demo char: %dx%d x%dy%d >%d (%u)\n"
                  ,mydef.width
                  ,mydef.height
                  ,mydef.xoffset
                  ,mydef.yoffset
                  ,mydef.devicewidth
                  ,setpixels);
            printnum--;
        }
        glyphs += 1;
        definitions.emplace(addr,mydef);
        
        cont:
        free(data);
    }
    printf("Total glyphs: %u\n",glyphs);
    
    auto test = fopen(outtxt.data(), "w");
    
    fprintf(test, "NAME=%s\n",myfont.NAME.data());
    fprintf(test, "FAMI=%s\n",myfont.FAMI.data());
    fprintf(test, "WEIG=%s\n",myfont.WEIG.data());
    fprintf(test, "SLAN=%s\n",myfont.SLAN.data());
    fprintf(test, "PTSZ=%d\n",myfont.PTSZ);
    fprintf(test, "MAXW=%d\n",myfont.MAXW);
    fprintf(test, "MAXH=%d\n",myfont.MAXH);
    fprintf(test, "ASCE=%d\n",myfont.ASCE);
    fprintf(test, "DESC=%d\n",myfont.DESC);
     
    u32 sumWidth = 0; // with 1px padding per glyph

    for(auto c : myfont.CHIX)
    {
        auto def=definitions[c.addr];
        sumWidth += def.width+1;
        fprintf(test, "CHAR:\n");
        fprintf(test, " CODEPOINT=U%X\n",c.codepoint);
        fprintf(test, " RIGHT=%d\n",def.xoffset);
        fprintf(test, " EXTRA=%d\n",signed(def.devicewidth)-def.width);
    }

    #define s_boxpad 1
    size_t d_cols = display; // display columns

    u32 dwidth = sumWidth;
    size_t height = myfont.ASCE+myfont.DESC;
    size_t cheight = height; // character height
    u32 dheight = height; // outimg height
    if(display)
    {
        dwidth = (myfont.MAXW+s_boxpad+2)*d_cols+s_boxpad;
        cheight = myfont.MAXH;
        height = cheight+s_boxpad+2;
        dheight = height*((myfont.CHIX.size()+(d_cols-1))/d_cols)+s_boxpad;
    }
    printf("%dx%d\n",dwidth,dheight);
    
    #define c_bg 0xFF606060
    #define c_fg 0xFFFFFFFF
    #define c_red 0xFF0000FF
    #define c_null 0xFF000000
    
    img myimage(dwidth,dheight,c_bg);
    u32 * pixels = (u32*)myimage.data;
    u32 x = 0;
    u32 cnt = 0;
    for(auto c : myfont.CHIX)
    {
        auto def = definitions[c.addr];
        int y=0;
        if(display)
        {
            // create bounding box
            y = (cnt/d_cols)*(s_boxpad+2+myfont.MAXH);
            for(size_t i = s_boxpad; i < s_boxpad+myfont.MAXW+2; i++)
            {
                pixels[x+i + (y+s_boxpad)*dwidth] = c_null;
                pixels[x+i + (y+s_boxpad+myfont.MAXH+1)*dwidth] = c_null;
            }
            for(size_t i = s_boxpad+1; i < s_boxpad+1+myfont.MAXH; i++)
            {
                pixels[x+s_boxpad + (y+i)*dwidth] = c_null;
                pixels[x+s_boxpad+myfont.MAXW+1 + (y+i)*dwidth] = c_null;
            }
            x += s_boxpad+1;
            y += s_boxpad+1;
        }
        int oy = myfont.ASCE-def.height-def.yoffset;
        if(oy < 0) printf("Font %d extends above ascent. Not all pixels will be included.\n", cnt+1);
        int ay = 0;
        int ox = display ? def.xoffset : 0;
        for(size_t i = 0; i < def.pixels.size(); i++)
        {
            size_t ax = i%def.width;
            ay = i/def.width;
            u32 fg=c_fg;
            u32 bg=c_bg;
            if(oy+ay < 0 || oy+ay >= cheight)
            {
                if(display && (oy+ay == -1 || oy+ay == myfont.MAXH))
                {
                    fg=c_red;
                    bg=c_null;
                }
                else
                {
                    continue;
                }
            }
            if(ox+ax < 0 || ox+ax >= myfont.MAXW)
            {
                if(display && (ox+ax == -1 || ox+ax == myfont.MAXW))
                {
                    fg=c_red;
                    bg=c_null;
                }
                else
                {
                    continue;
                }
            }
            pixels[x+ox+ax + (y+oy+ay)*dwidth] = def.pixels[i]?fg:bg;
        }
        if(oy+ay > myfont.MAXH) printf("Font %d extends below descent. Not all pixels will be included.\n", cnt+1);
        y += oy;
        x += display ? myfont.MAXW : def.width;
        if(! display)
        {
            for(size_t i = 0; i < height; i++)
            {
                pixels[x + i*dwidth] = c_null;
            }
        }
        x += 1;
        cnt += 1;
        if(display)
        {
            if(cnt%d_cols==0)
            {
                x = 0;
            }
        }
    }
    
    printf("%dx%dx%d\n", myimage.w, myimage.h, myimage.n);
    
    int err = stbi_write_bmp(outimg.data(), myimage.w, myimage.h, 4, myimage.data);
    
    printf("%d\n", err);
    
    puts("finished gracefully");
    
    return 0;
}
