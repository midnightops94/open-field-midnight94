#pragma once

namespace catcher_scene {

constexpr int unit=1024;
constexpr unsigned pulse_ticks=512, flies=3, grit_groups=14, grades=3;

struct Matrix { int xx,xy,yx,yy,x,y; };

static inline int multiply(int a,int b) { return (a*b+unit/2)>>10; }
static inline int normal(unsigned value) { return (value+16)>>5; }

static inline Matrix anchored(int xx,int xy,int yx,int yy,int cx,int cy,int to_x,int to_y) {
    return {xx,xy,yx,yy,to_x*unit-cx*xx-cy*yx,to_y*unit-cx*xy-cy*yy};
}

static inline unsigned phase(unsigned remaining) {
    const int t=(remaining>256u ? pulse_ticks-remaining : remaining)*4u;
    return multiply(multiply(t,t),3*unit-2*t);
}

static inline Matrix rotation(int angle,int x,int y,int target_x,int target_y) {
    // The poses are below 0.52 radians from the authored path. These Q10
    // polynomials stay within one part per thousand over the complete range.
    const int a2=multiply(angle,angle);
    const int sine=angle-multiply(multiply(a2,angle),171);
    const int cosine=unit-(a2>>1)+multiply(multiply(a2,a2),43);
    return anchored(cosine,sine,-sine,cosine,x,y,target_x,target_y);
}

static inline Matrix swing(unsigned value,unsigned eased) {
    const int u=normal(value);
    const int excursion=420-multiply(144,u);
    return rotation(-multiply(164,u-512)-multiply(excursion,eased),225,111,225,111);
}

static inline Matrix net_axes(int sx,int sy,int cx,int cy,int tx,int ty) {
    // Conjugate the local scales by the SVG's baked -0.29 rad grip angle.
    const int delta=sx-sy;
    const int shear=-multiply(delta,281);
    const int xx=sy+multiply(delta,940);
    return anchored(xx,shear,shear,sx+sy-xx,cx,cy,tx,ty);
}

static inline Matrix shaft(unsigned value) {
    return net_axes(1126-multiply(203,normal(value)),unit,225,111,225,111);
}

static inline Matrix net(unsigned value) {
    const int u=normal(value);
    const int offset=multiply(15,u-512);
    return net_axes(469+multiply(1109,u),558+multiply(931,u),299,89,
                    299+multiply(982,offset),89-multiply(293,offset));
}

static inline Matrix scarf(unsigned mix,unsigned scatter,unsigned eased) {
    const int reach=multiply(530,normal(mix))+multiply(53,normal(scatter));
    const int lift=multiply(76,normal(scatter))+multiply(82,eased);
    // Anchor the scarf at the collar while extending and lifting its loose end.
    return anchored(unit+reach,lift,0,unit,188,93,188,93);
}

static inline Matrix fly(unsigned index,unsigned scatter,unsigned eased,bool captured) {
    constexpr int x[]={312,334,307}, y[]={63,103,138};
    constexpr int dx[]={-4,38,36}, dy[]={-20,29,20};
    constexpr int angle[]={194,311,-408}, swing_x[]={5,4,-5}, swing_y[]={-3,8,-4};
    const int u=normal(scatter);
    const int turn=multiply(angle[index],multiply(u,u))+multiply(index==1 ? 103 : -103,eased);
    const int tx=x[index]+multiply(dx[index],u)+multiply(swing_x[index],eased)+(captured && index==0 ? 28 : 0);
    const int ty=y[index]+multiply(dy[index],u)+multiply(swing_y[index],eased);
    return rotation(turn,x[index],y[index],tx,ty);
}

static inline unsigned ink(unsigned grain) { return (32u*(32768u-grain)+16384u)>>15; }
static inline bool engraved(unsigned surface,unsigned grade,unsigned wear) {
    // Offset the three retained layers over different surfaces so the full
    // composition changes across the knob range without a single coarse jump.
    constexpr unsigned threshold[]={3,12,22};
    return wear>=threshold[grade]+((surface*7u+grade*3u)&7u);
}
static inline unsigned trails(unsigned mix) { return (3u*mix+25200u)>>15; }
static inline unsigned glow(unsigned mix) { return (3u*mix+28000u)>>15; }

} // namespace catcher_scene
