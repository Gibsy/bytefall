#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* layout */
#define WIN_W  800
#define WIN_H  560
#define BAR_H   36
#define BOT_H   38
#define SCR_W   10

/* colours */
#define C_BG        0x141416ff
#define C_BAR       0x222224ff
#define C_DIV       0x333336ff
#define C_TEXT      0xf2f2f4ff
#define C_MUTED     0x78787eff
#define C_BTN       0x38383cff
#define C_BTN_HOV   0x505056ff
#define C_BTN_BRD   0x555558ff
#define C_ACT_BG    0x061830ff
#define C_ACT       0x0a84ffff
#define C_GREEN     0x32d74bff
#define C_GREEN_BG  0x0c2216ff
#define C_RED       0xff3b30ff
#define C_RED_BG    0x2a0c0cff

typedef enum { MODE_8G=0, MODE_RGB=1, MODE_RGBA=2, MODE_16G=3, MODE_COUNT } PixelMode;
static const char *MODE_NAMES[] = {"8G","RGB","RGBA","16G"};
static const int   MODE_BPP[]   = {1,3,4,2};

/* audio */
#define AUDIO_RATE  8000
#define AUDIO_BUF   512

/* waterfall */
#define VIEW_W_PX   128   /* logical columns (fewer = bigger pixels) */
#define PIXEL_H       3   /* row height in screen px */
#define SCROLL_SPD    1   /* px per frame, must be <= PIXEL_H */

/* 5x7 bitmap font */
static const Uint8 F57[][5]={
{0,0,0,0,0},{0,0,95,0,0},{0,7,0,7,0},{20,127,20,127,20},
{36,42,127,42,18},{35,19,8,100,98},{54,73,85,34,80},{0,5,3,0,0},
{0,28,34,65,0},{0,65,34,28,0},{20,8,62,8,20},{8,8,62,8,8},
{0,80,48,0,0},{8,8,8,8,8},{0,96,96,0,0},{32,16,8,4,2},
{62,81,73,69,62},{0,66,127,64,0},{66,97,81,73,70},{33,65,69,75,49},
{24,20,18,127,16},{39,69,69,69,57},{60,74,73,73,48},{1,113,9,5,3},
{54,73,73,73,54},{6,73,73,41,30},{0,54,54,0,0},{0,86,54,0,0},
{8,20,34,65,0},{20,20,20,20,20},{0,65,34,20,8},{2,1,81,9,6},
{50,73,121,65,62},{126,17,17,17,126},{127,73,73,73,54},{62,65,65,65,34},
{127,65,65,34,28},{127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},
{127,8,8,8,127},{0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},
{127,64,64,64,64},{127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},
{127,9,9,9,6},{62,65,81,33,94},{127,9,25,41,70},{70,73,73,73,49},
{1,1,127,1,1},{63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},
{99,20,8,20,99},{7,8,112,8,7},{97,81,73,69,67},{0,127,65,65,0},
{2,4,8,16,32},{0,65,65,127,0},{4,2,1,2,4},{64,64,64,64,64},
{0,1,2,4,0},{32,84,84,84,120},{127,72,68,68,56},{56,68,68,68,32},
{56,68,68,72,127},{56,84,84,84,24},{8,126,9,1,2},{12,82,82,82,62},
{127,8,4,4,120},{0,68,125,64,0},{32,64,68,61,0},{127,16,40,68,0},
{0,65,127,64,0},{124,4,24,4,120},{124,8,4,4,120},{56,68,68,68,56},
{124,20,20,20,8},{8,20,20,24,124},{124,8,4,4,8},{72,84,84,84,32},
{4,63,68,64,32},{60,64,64,32,124},{28,32,64,32,28},{60,64,48,64,60},
{68,40,16,40,68},{12,80,80,80,60},{68,100,84,76,68}
};

static void set_col(SDL_Renderer*r,Uint32 rgba){
    SDL_SetRenderDrawColor(r,(rgba>>24)&0xff,(rgba>>16)&0xff,(rgba>>8)&0xff,rgba&0xff);
}
static void fill_rect(SDL_Renderer*r,int x,int y,int w,int h,Uint32 c){
    set_col(r,c);SDL_Rect rc={x,y,w,h};SDL_RenderFillRect(r,&rc);
}
static void draw_border(SDL_Renderer*r,int x,int y,int w,int h,Uint32 c){
    set_col(r,c);SDL_Rect rc={x,y,w,h};SDL_RenderDrawRect(r,&rc);
}
static void draw_char(SDL_Renderer*r,int x,int y,char c,int sc,Uint32 col){
    int i=(unsigned char)c-32;
    if(i<0||i>=(int)(sizeof(F57)/5))return;
    set_col(r,col);
    for(int cx=0;cx<5;cx++){
        Uint8 b=F57[i][cx];
        for(int ry=0;ry<7;ry++)
            if(b&(1<<ry)){SDL_Rect rc={x+cx*sc,y+ry*sc,sc,sc};SDL_RenderFillRect(r,&rc);}
    }
}
static void draw_str(SDL_Renderer*r,int x,int y,const char*s,int sc,Uint32 col){
    while(*s){draw_char(r,x,y,*s,sc,col);x+=6*sc;s++;}
}
static int str_w(const char*s,int sc){return(int)strlen(s)*6*sc;}

/* state */
static Uint8  *g_data   = NULL;
static size_t  g_size   = 0;
static char    g_name[256] = "";

static int       g_playing  = 0;
static double    g_pos      = 0.0;
static double    g_speed    = 1.0;
static int       g_scroll_y = 0;
static PixelMode g_mode     = MODE_RGB;

static SDL_AudioDeviceID g_adev      = 0;
static volatile size_t   g_audio_pos = 0;
static int               g_muted     = 0;
static float             g_volume    = 0.5f;

static int    g_show_help    = 0;
static int    g_scr_drag     = 0;
static int    g_scr_drag_y   = 0;
static double g_scr_drag_pos = 0;

static char   g_flash[160]  = "";
static Uint32 g_flash_until = 0;
static int    g_flash_ok    = 1;

/* audio callback: raw bytes as PCM */
static void audio_cb(void*ud,Uint8*stream,int len){
    (void)ud;
    if(!g_data||g_size==0||g_muted||!g_playing){SDL_memset(stream,0,len);return;}
    Sint16*out=(Sint16*)stream; int n=len/2;
    for(int i=0;i<n;i++){
        if(g_audio_pos>=g_size){out[i]=0;continue;}
        Sint16 s=(Sint16)((Sint8)g_data[g_audio_pos++]);
        out[i]=(Sint16)(s*(Sint16)(g_volume*255.0f));
    }
}
static void audio_init(void){
    SDL_AudioSpec want={0},got;
    want.freq=AUDIO_RATE;want.format=AUDIO_S16SYS;
    want.channels=1;want.samples=AUDIO_BUF;want.callback=audio_cb;
    g_adev=SDL_OpenAudioDevice(NULL,0,&want,&got,0);
    if(g_adev)SDL_PauseAudioDevice(g_adev,0);
}

static void do_flash(const char*m,int ok,int ms){
    snprintf(g_flash,sizeof(g_flash),"%s",m);
    g_flash_ok=ok;g_flash_until=SDL_GetTicks()+ms;
}
static void load_file(const char*path){
    FILE*fp=fopen(path,"rb");
    if(!fp){do_flash("Cannot open file",0,3000);return;}
    fseek(fp,0,SEEK_END);g_size=(size_t)ftell(fp);fseek(fp,0,SEEK_SET);
    if(g_data)free(g_data);
    g_data=(Uint8*)malloc(g_size?g_size:1);
    if(fread(g_data,1,g_size,fp)!=g_size){
        free(g_data);g_data=NULL;fclose(fp);do_flash("Read error",0,3000);return;
    }
    fclose(fp);
    const char*b=path;
    const char*sl=strrchr(path,'/'),*bs=strrchr(path,'\\');
    if(sl&&sl>=b)b=sl+1;if(bs&&bs>=b)b=bs+1;
    snprintf(g_name,sizeof(g_name),"%s",b);
    g_pos=0;g_scroll_y=0;g_playing=1;g_audio_pos=0;
    char buf[64];
    if(g_size>=(size_t)(1<<20)) snprintf(buf,sizeof(buf),"Loaded %.1f MB",(double)g_size/(1<<20));
    else if(g_size>=1024)       snprintf(buf,sizeof(buf),"Loaded %.1f KB",(double)g_size/1024);
    else                        snprintf(buf,sizeof(buf),"Loaded %zu B",g_size);
    do_flash(buf,1,2500);
}

static int row_stride(void){return VIEW_W_PX*MODE_BPP[g_mode];}

static void decode_row(size_t byte_start,Uint8*out_rgba){
    int bpp=MODE_BPP[g_mode];
    for(int col=0;col<VIEW_W_PX;col++){
        Uint8 rv=0,gv=0,bv=0;
        size_t bi=byte_start+(size_t)col*bpp;
        switch(g_mode){
            case MODE_8G:  if(bi<g_size)rv=gv=bv=g_data[bi]; break;
            case MODE_RGB:
                if(bi+0<g_size)rv=g_data[bi+0];
                if(bi+1<g_size)gv=g_data[bi+1];
                if(bi+2<g_size)bv=g_data[bi+2]; break;
            case MODE_RGBA:
                if(bi+0<g_size)rv=g_data[bi+0];
                if(bi+1<g_size)gv=g_data[bi+1];
                if(bi+2<g_size)bv=g_data[bi+2]; break;
            case MODE_16G:{
                Uint16 v=0;
                if(bi+0<g_size)v =(Uint16)(g_data[bi+0]<<8);
                if(bi+1<g_size)v|=(Uint16)(g_data[bi+1]);
                rv=gv=bv=(Uint8)(v>>8); break;
            }
            default: break;
        }
        out_rgba[col*4+0]=rv;out_rgba[col*4+1]=gv;
        out_rgba[col*4+2]=bv;out_rgba[col*4+3]=255;
    }
}

/* buttons */
#define SB_H   22
#define SB_PAD  4

#define BID_8G    0
#define BID_RGB   1
#define BID_RGBA  2
#define BID_16G   3
#define BID_RW    4
#define BID_PLAY  5
#define BID_FF    6
#define BID_MUTE  7
#define BID_HELP  8
#define BID_COUNT 9

typedef struct { SDL_Rect r; const char*lbl; int id; } Btn;
static Btn g_btns[BID_COUNT];
static int g_hov[BID_COUNT];

static int in_rect(int mx,int my,SDL_Rect b){
    return mx>=b.x&&mx<b.x+b.w&&my>=b.y&&my<b.y+b.h;
}
static void draw_btn(SDL_Renderer*r,SDL_Rect b,const char*lbl,int hov,int active){
    Uint32 bg=active?C_ACT_BG:(hov?C_BTN_HOV:C_BTN);
    Uint32 brd=active?C_ACT:(hov?0x8888aaff:C_BTN_BRD);
    Uint32 fg=active?C_ACT:C_TEXT;
    fill_rect(r,b.x,b.y,b.w,b.h,bg);
    draw_border(r,b.x,b.y,b.w,b.h,brd);
    int tw=str_w(lbl,1);
    draw_str(r,b.x+(b.w-tw)/2,b.y+(b.h-7)/2,lbl,1,fg);
}

static void layout_buttons(int ww,int wh){
    int y=wh-BOT_H+(BOT_H-SB_H)/2;
    int mode_w[]={18,22,26,18};
    int x=SB_PAD*2;
    for(int m=0;m<4;m++){
        int w=mode_w[m]+10;
        g_btns[BID_8G+m]=(Btn){{x,y,w,SB_H},MODE_NAMES[m],BID_8G+m};
        x+=w+SB_PAD;
    }
    int tw_rw=22,tw_play=38,tw_ff=22;
    int total=tw_rw+SB_PAD+tw_play+SB_PAD+tw_ff;
    int cx=(ww-total)/2;
    g_btns[BID_RW]  =(Btn){{cx,y,tw_rw,  SB_H},"<<",  BID_RW};   cx+=tw_rw+SB_PAD;
    g_btns[BID_PLAY]=(Btn){{cx,y,tw_play,SB_H},"PLAY",BID_PLAY}; cx+=tw_play+SB_PAD;
    g_btns[BID_FF]  =(Btn){{cx,y,tw_ff,  SB_H},">>",  BID_FF};
    int rx=ww-SB_PAD*2;
    g_btns[BID_HELP]=(Btn){{rx-20,y,20,SB_H},"?",   BID_HELP}; rx-=20+SB_PAD;
    g_btns[BID_MUTE]=(Btn){{rx-32,y,32,SB_H},"MUTE",BID_MUTE};
}

static void draw_waterfall(SDL_Renderer*ren,int ww,int wh){
    int vx=0,vy=BAR_H,vw=ww-SCR_W,vh=wh-BAR_H-BOT_H;
    if(vh<=0||vw<=0)return;

    int stride=row_stride();
    float scale=(float)vw/(float)VIEW_W_PX;
    int cell_h=PIXEL_H;
    int rows=vh/cell_h+2;
    size_t base=(size_t)g_pos;
    base=(stride>0)?(base/stride)*stride:0;

    SDL_Rect clip={vx,vy,vw,vh}; SDL_RenderSetClipRect(ren,&clip);

    static Uint8*rowbuf=NULL; static int rowbuf_w=0;
    if(rowbuf_w!=VIEW_W_PX){
        if(rowbuf)free(rowbuf);
        rowbuf=(Uint8*)malloc(VIEW_W_PX*4); rowbuf_w=VIEW_W_PX;
    }

    for(int row=0;row<rows;row++){
        int rfb=rows-1-row;
        int yp=vy+row*cell_h-g_scroll_y;
        if(yp+cell_h<=vy||yp>vy+vh)continue;
        float age=1.0f-(float)rfb/(float)(rows>1?rows-1:1);
        long long bs=(long long)base-(long long)rfb*stride;
        if(bs<0){ fill_rect(ren,vx,yp,vw,cell_h,C_BG); continue; }
        decode_row((size_t)bs,rowbuf);
        for(int col=0;col<VIEW_W_PX;col++){
            Uint8 rv=(Uint8)(rowbuf[col*4+0]*age);
            Uint8 gv=(Uint8)(rowbuf[col*4+1]*age);
            Uint8 bv=(Uint8)(rowbuf[col*4+2]*age);
            int px=(int)(col*scale);
            int pw=(int)((col+1)*scale)-px; if(pw<1)pw=1;
            SDL_SetRenderDrawColor(ren,rv,gv,bv,255);
            SDL_Rect rc={vx+px,yp,pw,cell_h}; SDL_RenderFillRect(ren,&rc);
        }
    }

    set_col(ren,0xffffff28);
    SDL_RenderDrawLine(ren,vx,vy+vh-cell_h,vx+vw,vy+vh-cell_h);
    SDL_RenderSetClipRect(ren,NULL);

    if(g_size>0){
        int bx=0,by=vy+vh,bw=vw,bh=2;
        fill_rect(ren,bx,by,bw,bh,C_BTN);
        int filled=(int)((double)bw*(g_pos/(double)g_size));
        if(filled>bw)filled=bw;
        fill_rect(ren,bx,by,filled,bh,C_ACT);
    }

    /* scrollbar */
    int sx=ww-SCR_W,sy=vy,sh=vh;
    fill_rect(ren,sx,sy,SCR_W,sh,0x1e1e20ff);
    if(g_size>0){
        int thumb_h=sh/8; if(thumb_h<20)thumb_h=20; if(thumb_h>sh)thumb_h=sh;
        double frac=g_pos/(double)g_size;
        int thumb_y=sy+(int)(frac*(sh-thumb_h));
        if(thumb_y<sy)thumb_y=sy;
        if(thumb_y+thumb_h>sy+sh)thumb_y=sy+sh-thumb_h;
        int mx2,my2; SDL_GetMouseState(&mx2,&my2);
        int on_thumb=(mx2>=sx&&mx2<sx+SCR_W&&my2>=thumb_y&&my2<thumb_y+thumb_h);
        fill_rect(ren,sx+2,thumb_y,SCR_W-4,thumb_h,(g_scr_drag||on_thumb)?0x6060aaff:0x44444aff);
    }
}

static void draw_help(SDL_Renderer*r,int ww,int wh){
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r,0x14,0x14,0x16,0xd0);
    SDL_Rect full={0,0,ww,wh}; SDL_RenderFillRect(r,&full);
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);

    int pw=320,ph=230,px=(ww-pw)/2,py=(wh-ph)/2;
    fill_rect(r,px,py,pw,ph,C_BAR);
    draw_border(r,px,py,pw,ph,C_DIV);
    draw_border(r,px+1,py+1,pw-2,ph-2,0x444448ff);

    const char*title="Keyboard Shortcuts";
    draw_str(r,px+(pw-str_w(title,1))/2,py+10,title,1,C_TEXT);
    fill_rect(r,px+12,py+22,pw-24,1,C_DIV);

    static const char*keys[][2]={
        {"Space",       "Play / Pause"},
        {"R",           "Restart"},
        {"M",           "Mute / Unmute"},
        {"Up / Down",   "Speed x2 / x0.5"},
        {"Mouse Wheel", "Scroll / Seek"},
        {"Scrollbar",   "Click or drag to seek"},
        {"Drag & Drop", "Load any file"},
    };
    int ny=py+30;
    for(int i=0;i<7;i++){
        draw_str(r,px+16,ny,keys[i][0],1,C_ACT);
        draw_str(r,px+140,ny,keys[i][1],1,C_MUTED);
        ny+=18;
    }
    const char*cl="[?] or [Esc] to close";
    draw_str(r,px+(pw-str_w(cl,1))/2,py+ph-16,cl,1,C_MUTED);
}

int main(int argc,char**argv){
    SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO);
    SDL_Window*win=SDL_CreateWindow("Binary Waterfall",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,WIN_W,WIN_H,
        SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Renderer*ren=SDL_CreateRenderer(win,-1,
        SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);

    audio_init();
    if(argc>=2)load_file(argv[1]);

    int running=1; SDL_Event ev;
    while(running){
        int mx,my; SDL_GetMouseState(&mx,&my);
        int ww,wh; SDL_GetWindowSize(win,&ww,&wh);

        layout_buttons(ww,wh);
        for(int i=0;i<BID_COUNT;i++)
            g_hov[i]=in_rect(mx,my,g_btns[i].r);

        while(SDL_PollEvent(&ev)){
            if(ev.type==SDL_QUIT) running=0;
            if(ev.type==SDL_KEYDOWN){
                if(g_show_help){
                    if(ev.key.keysym.sym==SDLK_ESCAPE||ev.key.keysym.sym==SDLK_SLASH
                       ||ev.key.keysym.sym==SDLK_QUESTION||ev.key.keysym.sym==SDLK_h)
                        g_show_help=0;
                    break;
                }
                switch(ev.key.keysym.sym){
                    case SDLK_ESCAPE: case SDLK_q: running=0; break;
                    case SDLK_SPACE:  g_playing=!g_playing; break;
                    case SDLK_r:      g_pos=0;g_scroll_y=0;g_playing=1;g_audio_pos=0; break;
                    case SDLK_m:      g_muted=!g_muted; break;
                    case SDLK_h: case SDLK_SLASH: g_show_help=!g_show_help; break;
                    case SDLK_UP:   g_speed*=2.0;if(g_speed>32)g_speed=32; break;
                    case SDLK_DOWN: g_speed*=0.5;if(g_speed<0.125)g_speed=0.125; break;
                }
            }
            if(ev.type==SDL_MOUSEBUTTONDOWN&&ev.button.button==SDL_BUTTON_LEFT){
                if(g_show_help){g_show_help=0;break;}
                int bx=ev.button.x,by=ev.button.y;
                int vy=BAR_H,vh=wh-BAR_H-BOT_H,sx=ww-SCR_W;
                if(bx>=sx&&g_size>0){
                    int sh=vh;
                    int thumb_h=sh/8; if(thumb_h<20)thumb_h=20;
                    int thumb_y=vy+(int)((g_pos/(double)g_size)*(sh-thumb_h));
                    if(by>=thumb_y&&by<thumb_y+thumb_h){
                        g_scr_drag=1;g_scr_drag_y=by;g_scr_drag_pos=g_pos;
                    } else {
                        double nf=(double)(by-vy)/(double)sh;
                        if(nf<0)nf=0;if(nf>1)nf=1;
                        g_pos=nf*(double)g_size;g_audio_pos=(size_t)g_pos;
                    }
                    break;
                }
                if(g_hov[BID_PLAY]) g_playing=!g_playing;
                if(g_hov[BID_RW]){int s=row_stride();g_pos-=s*30;if(g_pos<0)g_pos=0;g_audio_pos=(size_t)g_pos;}
                if(g_hov[BID_FF]&&g_size){int s=row_stride();g_pos+=s*30;g_audio_pos=(size_t)g_pos;}
                if(g_hov[BID_MUTE]) g_muted=!g_muted;
                if(g_hov[BID_HELP]) g_show_help=!g_show_help;
                for(int m=0;m<4;m++) if(g_hov[BID_8G+m]) g_mode=(PixelMode)m;
                (void)bx;(void)by;
            }
            if(ev.type==SDL_MOUSEBUTTONUP&&ev.button.button==SDL_BUTTON_LEFT)
                g_scr_drag=0;
            if(ev.type==SDL_MOUSEMOTION&&g_scr_drag&&g_size>0){
                int sh=wh-BAR_H-BOT_H;
                int thumb_h=sh/8; if(thumb_h<20)thumb_h=20;
                double delta=(double)(ev.motion.y-g_scr_drag_y)/(double)(sh-thumb_h);
                g_pos=g_scr_drag_pos+delta*(double)g_size;
                if(g_pos<0)g_pos=0;
                if(g_pos>(double)g_size)g_pos=(double)g_size;
                g_audio_pos=(size_t)g_pos;
            }
            if(ev.type==SDL_MOUSEWHEEL&&!g_show_help){
                int s=row_stride();
                g_pos-=ev.wheel.y*s*5;
                if(g_pos<0)g_pos=0;
                g_audio_pos=(size_t)g_pos;
            }
            if(ev.type==SDL_DROPFILE){load_file(ev.drop.file);SDL_free(ev.drop.file);}
        }

        if(g_playing&&g_data&&g_size>0&&!g_show_help){
            int stride=row_stride();
            g_scroll_y+=SCROLL_SPD;
            if(g_scroll_y>=PIXEL_H){
                g_scroll_y-=PIXEL_H;
                g_pos+=(double)stride*g_speed;
                if(g_pos>=(double)g_size){
                    g_pos=(double)g_size;g_playing=0;
                    do_flash("End of file",1,2000);
                }
            }
        }

        set_col(ren,C_BG); SDL_RenderClear(ren);

        /* top bar */
        fill_rect(ren,0,0,ww,BAR_H,C_BAR);
        fill_rect(ren,0,BAR_H-1,ww,1,C_DIV);
        if(g_name[0]){
            int tw=str_w(g_name,2),tx=(ww-tw)/2; if(tx<8)tx=8;
            draw_str(ren,tx,BAR_H/2-7,g_name,2,C_TEXT);
            char sz[32];
            if(g_size>=(size_t)(1<<20)) snprintf(sz,sizeof(sz),"%.1f MB",(double)g_size/(1<<20));
            else if(g_size>=1024)       snprintf(sz,sizeof(sz),"%.1f KB",(double)g_size/1024);
            else                        snprintf(sz,sizeof(sz),"%zu B",g_size);
            draw_str(ren,ww-str_w(sz,1)-8,BAR_H/2-3,sz,1,C_MUTED);
        } else {
            const char*h="Drop any file here";
            draw_str(ren,(ww-str_w(h,1))/2,BAR_H/2-3,h,1,C_MUTED);
        }

        draw_waterfall(ren,ww,wh);
        if(!g_data){
            const char*m="Drop any file onto the window";
            draw_str(ren,(ww-str_w(m,1))/2,wh/2-3,m,1,C_MUTED);
        }

        /* bottom bar */
        fill_rect(ren,0,wh-BOT_H,ww,BOT_H,C_BAR);
        fill_rect(ren,0,wh-BOT_H,ww,1,C_DIV);

        for(int m=0;m<4;m++)
            draw_btn(ren,g_btns[BID_8G+m].r,MODE_NAMES[m],g_hov[BID_8G+m],(g_mode==m));
        draw_btn(ren,g_btns[BID_RW].r,  "<<",              g_hov[BID_RW],  0);
        draw_btn(ren,g_btns[BID_PLAY].r,g_playing?"||":"|>",g_hov[BID_PLAY],0);
        draw_btn(ren,g_btns[BID_FF].r,  ">>",              g_hov[BID_FF],  0);
        draw_btn(ren,g_btns[BID_MUTE].r,g_muted?"(M)":"MUT",g_hov[BID_MUTE],g_muted);
        draw_btn(ren,g_btns[BID_HELP].r,"?",               g_hov[BID_HELP],g_show_help);

        char spbuf[12]; snprintf(spbuf,sizeof(spbuf),"x%.3g",g_speed);
        SDL_Rect mr=g_btns[BID_MUTE].r;
        draw_str(ren,mr.x+mr.w+5,mr.y+(SB_H-7)/2,spbuf,1,C_MUTED);

        if(SDL_GetTicks()<g_flash_until){
            Uint32 bc=g_flash_ok?C_GREEN_BG:C_RED_BG,tc=g_flash_ok?C_GREEN:C_RED;
            int fw=str_w(g_flash,1)+24,fh=20;
            int fx=ww-fw-10,fy=wh-BOT_H-fh-6;
            fill_rect(ren,fx,fy,fw,fh,bc);
            draw_border(ren,fx,fy,fw,fh,tc);
            draw_str(ren,fx+12,fy+(fh-7)/2,g_flash,1,tc);
        }

        if(g_show_help) draw_help(ren,ww,wh);

        SDL_RenderPresent(ren);
        SDL_Delay(8);
    }

    if(g_adev)SDL_CloseAudioDevice(g_adev);
    if(g_data)free(g_data);
    SDL_DestroyRenderer(ren);SDL_DestroyWindow(win);SDL_Quit();
    return 0;
}