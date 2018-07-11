#include <stdio.h>
#include <SDL2/SDL.h>
#include <vector>
#include <string>
#include <map>

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

std::string get_string(FILE* f)
{
    u32 len;
    fread(&len,4,1,f);
    len = SDL_SwapBE32(len);
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
    font->PTSZ = SDL_SwapBE16(font->PTSZ);
    printf("Font pt size: %d\n",font->PTSZ);
}
void loadMAXW(PFF2* font, FILE* f)
{
    fread(&font->MAXW,2,1,f);
    font->MAXW = SDL_SwapBE16(font->MAXW);
    printf("Font max width: %d\n",font->MAXW);
}
void loadMAXH(PFF2* font, FILE* f)
{
    fread(&font->MAXH,2,1,f);
    font->MAXH = SDL_SwapBE16(font->MAXH);
    printf("Font max height: %d\n",font->MAXH);
}
void loadASCE(PFF2* font, FILE* f)
{
    fread(&font->ASCE,2,1,f);
    font->ASCE = SDL_SwapBE16(font->ASCE);
    printf("Font ascent: %d\n",font->ASCE);
}
void loadDESC(PFF2* font, FILE* f)
{
    fread(&font->DESC,2,1,f);
    font->DESC = SDL_SwapBE16(font->DESC);
    printf("Font descent: %d\n",font->DESC);
}
void nextCHIX(PFF2* font, FILE* f)
{
    chix mychix;
    fread(&mychix.codepoint,4,1,f);
    fread(&mychix.flags,1,1,f);
    fread(&mychix.addr,4,1,f);
    mychix.codepoint=SDL_SwapBE32(mychix.codepoint);
    mychix.addr=SDL_SwapBE32(mychix.addr);
    font->CHIX.push_back(mychix);
}
u32 count = 0;
void loadCHIX(PFF2* font, FILE* f)
{
    u32 size;
    fread(&size,4,1,f);
    size = SDL_SwapBE32(size);
    count = size/9;
    for(uint i=0; i<count; i++)
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
    printf("   -c # - bmp is character cells in # columns\n");
    exit(rc);
}

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
    std::string outbmp(argv[i+1]);
    outbmp += ".bmp";
    
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
    if(SDL_SwapBE32(data)!=4) goto invalid;
    
    fread(&data, 4, 1, f);
    if(data!=*(u32*)"PFF2") goto invalid;
    
    #define check_invalid_simple_u16 {u32 junk; fread(&junk,4,1,f); if(SDL_SwapBE32(junk)!=2) goto invalid;}
    
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
        mydef.width = SDL_Swap16(mydef.width);
        mydef.height = SDL_Swap16(mydef.height);
        mydef.xoffset = SDL_Swap16(mydef.xoffset);
        mydef.yoffset = SDL_Swap16(mydef.yoffset);
        mydef.devicewidth = SDL_Swap16(mydef.devicewidth);
        uint length = (mydef.width*mydef.height+7)/8;
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
    uint d_cols = display; // display columns

    u32 dwidth = sumWidth;
    uint height = myfont.ASCE+myfont.DESC;
    uint cheight = height; // character height
    u32 dheight = height; // bmp height
    if(display)
    {
        dwidth = (myfont.MAXW+s_boxpad+2)*d_cols+s_boxpad;
        cheight = myfont.MAXH;
        height = cheight+s_boxpad+2;
        dheight = height*((myfont.CHIX.size()+(d_cols-1))/d_cols)+s_boxpad;
    }
    printf("%dx%d\n",dheight,dwidth);
    
    #define c_bg 0x00606060
    #define c_fg 0x00FFFFFF
    #define c_red 0x00FF0000
    #define c_null 0x00000000
    
    SDL_Surface * myimage = SDL_CreateRGBSurface(0,dwidth,dheight,32,0,0,0,0);
    SDL_FillRect(myimage, NULL, c_bg);
    u32 * pixels = (u32*)myimage->pixels;
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
            for(uint i = s_boxpad; i < s_boxpad+myfont.MAXW+2; i++)
            {
                pixels[x+i + (y+s_boxpad)*dwidth] = c_null;
                pixels[x+i + (y+s_boxpad+myfont.MAXH+1)*dwidth] = c_null;
            }
            for(uint i = s_boxpad+1; i < s_boxpad+1+myfont.MAXH; i++)
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
        for(uint i = 0; i < def.pixels.size(); i++)
        {
            int ax = i%def.width;
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
            for(uint i = 0; i < height; i++)
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
    SDL_SaveBMP(myimage, outbmp.data());
    SDL_FreeSurface(myimage);
    
    return 0;
}
