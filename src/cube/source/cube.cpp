#include "display.hpp"
#include "cube-engine.hpp"
#include "cube_points.hpp"
extern "C" const Op1fCard op1f_card_cube OP1F_NAMED("op1f_card_cube");
namespace {
struct State { cube_field::Engine engine; cube_field::ResonanceRoom room; };
static_assert(sizeof(State)==768,"Cube effect state");
volatile unsigned place,shown;
fx_page::OffNotice notice{};
struct Point { int x,y; };
int lerp(int a,int b,unsigned amount) { return a+(((b-a)*int(amount)+16384)>>15); }
Point surface(unsigned row,unsigned col,unsigned shape,unsigned morph) {
    using cube_native_screen::points;
    const unsigned base=3*(shape>>14)+(morph>>14);
    shape=(shape&16383)*2;morph=(morph&16383)*2;
    int xy[2];
    for(unsigned i=0;i<2;++i) {
        int a=lerp(points[base][row][col][i],points[base+1][row][col][i],morph);
        int b=lerp(points[base+3][row][col][i],points[base+4][row][col][i],morph);
        xy[i]=lerp(a,b,shape);
    }
    return {xy[0],xy[1]};
}
// The Field composes the SVG first, then our canvas. Reserve the rounded status
// card in the wireframe and shade its other strokes as the retained dimmer does.
static bool outside_fx_card(int x,int y) {
    const int dx=x<120?120-x:x>360?x-360:0;
    const int dy=y<87?87-y:y>133?y-133:0;
    // Twelve-and-a-half-pixel corners plus one pixel of antialias coverage.
    return dx*dx+dy*dy>182;
}
void line(void* c,Point a,Point b,int scale,unsigned color) {
    const int tx=240*(1024-scale), ty=114*1024-110*scale;
    int x=((a.x*scale>>6)+tx+512)>>10, y=((a.y*scale>>6)+ty+512)>>10;
    const int end_x=((b.x*scale>>6)+tx+512)>>10, end_y=((b.y*scale>>6)+ty+512)>>10;
    // A retained SVG is painted before canvas calls. Split crossing strokes
    // at the notice boundary so the wireframe stays behind its border and text.
    if(notice.visible==1) {
        // RGB565 with each channel attenuated independently.
        const unsigned red=(color>>11)&31u, green=(color>>5)&63u, blue=color&31u;
        color=(((red+2u)*205u)>>10)<<11 | (((green+2u)*205u)>>10)<<5 | (((blue+2u)*205u)>>10);
    }
    if(notice.visible!=1 || (x<106 && end_x<106) || (x>374 && end_x>374) ||
       (y<73 && end_y<73) || (y>147 && end_y>147)) {
        op1f_paint_line(c,x,y,end_x,end_y,1,color);
        return;
    }
    const int dx=x<end_x?end_x-x:x-end_x, dy=y<end_y?end_y-y:y-end_y;
    const int sx=x<end_x?1:-1, sy=y<end_y?1:-1;
    int err=dx-dy,previous_x=x,previous_y=y,first_x=x,first_y=y;
    bool drawing=false;
    for(;;) {
        const bool outside=outside_fx_card(x,y);
        if(outside && !drawing) {first_x=x;first_y=y;}
        if(!outside && drawing) op1f_paint_line(c,first_x,first_y,previous_x,previous_y,1,color);
        drawing=outside;
        if(x==end_x && y==end_y) break;
        previous_x=x;previous_y=y;
        const int twice=2*err;
        if(twice > -dy){err-=dy;x+=sx;}
        if(twice < dx){err+=dx;y+=sy;}
    }
    if(drawing) op1f_paint_line(c,first_x,first_y,x,y,1,color);
}
void draw(void* c,const unsigned (&v)[4],bool has_picture) {
    if(!has_picture) op1f_paint_box(c,0,0,480,224,OP1F_COLOUR(13,18,18));
    // Mix changes the perspective box from a compact dry room to its full wet size.
    // Place the resonator in the same coordinate frame so it stays in the room.
    const int scale=592+int((384*v[3]+16384)>>15);
    #include "frame.inc"
    const int strength=int(v[2]);
    const int body_scale=(scale*(656+((368*strength+16384)>>15))+512)>>10;
    // A standing wave pinches the inner bands while the two lips retain their outline.
    static const int compression[]={0,200,32,300,32,200,0};
    const unsigned ribs[]={0,3,5,8,11,13};
    Point previous[16], now[16], center{};
    for(unsigned row=0;row<7;++row) {
        Point middle{};
        for(unsigned i=0;i<16;++i) {
            now[i]=surface(row,i,v[0],v[1]);
            middle.x+=now[i].x;
            middle.y+=now[i].y;
        }
        middle.x>>=4;middle.y>>=4;
        const int radial=1024-((compression[row]*strength+16384)>>15);
        for(Point& p:now) {
            p.x=middle.x+(((p.x-middle.x)*radial+512)>>10);
            p.y=middle.y+(((p.y-middle.y)*radial+512)>>10);
        }
        for(unsigned i=0;i<16;++i) line(c,now[i],now[(i+1)&15],body_scale,OP1F_COLOUR(110,122,118));
        if(row) for(unsigned i:ribs) line(c,previous[i],now[i],body_scale,OP1F_COLOUR(83,100,96));
        for(unsigned i=0;i<16;++i) previous[i]=now[i];
    }
    for(auto p:now) {center.x+=p.x;center.y+=p.y;}
    center.x/=16;center.y/=16;
    for(unsigned echo=0;echo<2;++echo) if(v[2]>(echo?4600u:1500u)) {
        const unsigned shrink=(v[2]*(echo?12000:6000)+16384)>>15;
        for(unsigned i=0;i<16;++i) previous[i]={lerp(now[i].x,center.x,shrink),lerp(now[i].y,center.y,shrink)};
        for(unsigned i=0;i<16;++i) line(c,previous[i],previous[(i+1)&15],body_scale,OP1F_COLOUR(145,152,147));
    }
}
}
extern "C" void cube_reset(unsigned,void* raw) OP1F_NAMED("cube_reset");
extern "C" void cube_reset(unsigned,void* raw) {
    if(!raw) return;
    auto& s=*static_cast<State*>(raw);
    cube_field::reset(s.engine,44100);cube_field::clear_room(s.room);s.room.level=0;
}
extern "C" void cube_process(const Op1fEffectBlock* b) OP1F_NAMED("cube_process");
extern "C" void cube_process(const Op1fEffectBlock* b) {
    if(!b || b->bytes<44 || !b->state || !b->on || b->frames!=64 || !b->values || !b->left || !b->right) return;
    auto& s=*static_cast<State*>(b->state);
    if(b->fresh||s.engine.rate!=44100) cube_reset(b->place,b->state);
    cube_field::Controls p{uint16_t(fx_page::q15(b->values[0])),uint16_t(fx_page::q15(b->values[1])),
                           uint16_t(fx_page::q15(b->values[2])),uint16_t(fx_page::q15(b->values[3]))};
    cube_field::update(s.engine,p);
    auto l=reinterpret_cast<int32_t*>(b->left),r=reinterpret_cast<int32_t*>(b->right);
    // update() sets B2 to the exact negative of B0 for every band.
    cube_field::process<true>(s.engine,l,r,l,r,64,p.mix,&s.room,cube_field::room_level(p.reso),p.morph);
}
extern "C" void cube_shown(unsigned where,unsigned how) OP1F_NAMED("cube_shown");
extern "C" void cube_shown(unsigned where,unsigned how) {place=where;shown=how;}
extern "C" void cube_frame(const Op1fFrame* f) OP1F_NAMED("cube_frame");
extern "C" void cube_frame(const Op1fFrame* f) {
    if(f->bytes<40 || !f->canvas || f->page!=&op1f_card_cube || !(shown&OP1F_SHOWN_UP)) return;
    static bool tried,installed;
    static void* document;
    if(!tried && !f->disk) {
        tried=true;document=op1f_picture(&op1f_card_cube,"cube_off.svg");
        if(!document) document=op1f_picture(&op1f_card_cube,"chamber.svg");
        if(document) notice.bind(document);
    }
    if(document && !installed) installed=op1f_page_stand_in(&op1f_card_cube,0,document)!=0;
    const bool off=notice.update(shown | (f->popup?OP1F_SHOWN_COVERED:0),installed);
    unsigned v[4];
    if(fx_page::values(&op1f_card_cube,place,v)) {
        draw(f->canvas,v,installed);
        static const char* const names[]={"SHAPE","MORPH","RESO","MIX"};
        fx_page::captions(f->canvas,installed?document:nullptr,names,v);
    }
    notice.fallback(f->canvas,off);
}
