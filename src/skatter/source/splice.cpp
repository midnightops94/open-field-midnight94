#include "splice.hpp"
#include <limits.h>
namespace splice {
static int32_t sat(int64_t x){return x>INT32_MAX?INT32_MAX:x<INT32_MIN?INT32_MIN:int32_t(x);}
static uint32_t min(uint32_t a,uint32_t b){return a<b?a:b;}
static uint32_t ceil16(uint64_t x){return uint32_t((x+65535)>>16);}
static uint32_t mul30(uint32_t a,uint32_t b){return uint32_t((uint64_t(a)*b)>>30);}
// Each admitted Field history is a 32,768-frame full-rate int16 stereo ring.
constexpr uint32_t ring_mask = full.frames - 1;
constexpr uint32_t phase_mask = (full.frames << 16) - 1;
#if defined(__bfin__)
struct Product { uint32_t low, high; };

static inline Product product(int32_t sample, uint32_t gain) {
    Product result;
    asm("(R1:0) = %2 * %3 (IS,NS);" : "=q0"(result.low), "=q1"(result.high)
        : "d"(sample), "d"(gain) : "ASTAT");
    return result;
}
#endif

static int32_t weighted(int32_t sample, uint32_t gain) {
#if defined(__bfin__)
    const Product p = product(sample, gain);
    return static_cast<int32_t>((p.low >> 30) | (p.high << 2));
#else
    return static_cast<int32_t>((static_cast<int64_t>(sample) * gain) >> 30);
#endif
}

// For positive gain each weighted Q31 sample is in [-2*gain, 2*gain).
// Normalization bounds the stereo sums to signed Q31 before the dry/wet blend.
static int32_t blend(int32_t dry, int32_t wet, uint32_t wet_gain) {
#if defined(__bfin__)
    const Product a = product(dry, Engine::one - wet_gain);
    const Product b = product(wet, wet_gain);
    const uint32_t low = a.low + b.low;
    const uint32_t high = a.high + b.high + (low < a.low);
    return static_cast<int32_t>((low >> 30) | (high << 2));
#else
    return static_cast<int32_t>((int64_t(dry)*(Engine::one-wet_gain) + int64_t(wet)*wet_gain) >> 30);
#endif
}
bool Engine::bind(void*mem,size_t n,Profile p,uint32_t rate){
 if(ring_||!mem||rate!=44100||p.frames!=full.frames||p.bits!=full.bits||p.decimation!=full.decimation||n<bytes(full))return false;
 const uintptr_t a=reinterpret_cast<uintptr_t>(mem),b=reinterpret_cast<uintptr_t>(this);
 if(a>UINTPTR_MAX-bytes(p)||(a<b+sizeof(*this)&&b<a+bytes(p)))return false;

 ring_=static_cast<uint8_t*>(mem);reset();return true;
}
void Engine::controls(const Controls&c){c_=c;
 auto clamp=[](uint32_t x,uint32_t lo,uint32_t hi){return x<lo?lo:x>hi?hi:x;};
 c_.window=clamp(c.window,2646,22050);c_.grain=clamp(c.grain,529,7056);c_.speed=clamp(c.speed,32768,131072);
 c_.wu=min(c.wu,one);c_.gu=min(c.gu,one);c_.scatter=min(c.scatter,one);c_.mix=min(c.mix,one);c_.direction=c.direction>2?1:c.direction;
 random_threshold_=uint32_t(uint64_t(c_.scatter)*4u/5u*4u);
 for(unsigned n=0;n<3;++n){
  const uint32_t speed=n==0 ? (3*c_.speed)>>2 : n==1 ? c_.speed : (3*c_.speed)>>1;
  const uint32_t length=min(c_.grain,(c_.window<<16)/(2*(speed>65536?speed:65536)));
  // Length is bounded by window/(2*speed), so the Q16 product fits in a word.
  constexpr uint32_t guard=(44100u*15+9999)/10000+2;
  launch_[n]={speed,length,((length*speed+65535)>>16)+2+guard};
 }
}
void Engine::reset(){write_=valid_=mix_=count_=repeats_=0;written_=saved_at_=0;saved_=was_hold_=was_bypass_=false;rng_=0x51a1ce;stats_={};}
uint32_t Engine::random(){uint32_t x=rng_;x^=x<<13;x^=x>>17;x^=x<<5;return rng_=x;}
void Engine::store(unsigned ch,int32_t x){
 int32_t q=(x>>16)+int32_t((uint32_t(x)>>15)&1u);
 if(q>32767)q=32767;
 const uint16_t u=uint16_t(q);
 const size_t i=2*(ch*full.frames+write_);
 ring_[i]=uint8_t(u); ring_[i+1]=uint8_t(u>>8);
}
int32_t Engine::read(unsigned ch,uint32_t pos)const{
 const uint32_t i=(pos>>16)&ring_mask,j=(i+1)&ring_mask;
 const int32_t f=static_cast<int32_t>(pos&65535u);
 const auto get=[&](uint32_t index){
  const size_t n=2*(ch*full.frames+index);
  return static_cast<int32_t>(static_cast<int16_t>(uint16_t(ring_[n])|(uint16_t(ring_[n+1])<<8)));
 };
 // The interpolation is convex and signed PCM16 scaled to Q31 fits int32.
 return get(i)*(65536-f)+get(j)*f;
}
void Engine::recut_block(Recut,const ClockBlock&,unsigned) {
 // The Field role has no qualified transport phase and uses Free throughout.
}
void Engine::launch(uint32_t fade){
 const uint32_t sc=min(c_.scatter,one),w=min(valid_,c_.window),guard=(44100u*15+9999)/10000+2;
 const bool pitched=random()<random_threshold_;
 const unsigned n=pitched ? uint32_t((uint64_t(random())*3)>>32) : 1u;
 const Launch& candidate=launch_[n];
 uint32_t speed=candidate.speed;
 const uint32_t len=candidate.length,minage=candidate.age;
 if(!len||w<minage){stats_.refusals++;return;}
 const uint32_t bias=uint32_t(161061274u+mul30(375809638u,c_.wu)); // .15+.35*u
 const uint32_t ordered=(minage<<16)+uint32_t((uint64_t(w-minage)*bias)>>14);
 const uint32_t reordered=(minage<<16)+uint32_t((uint64_t(random())*(w-minage))>>16);
 uint32_t age=uint32_t((uint64_t(ordered)*(one-sc)+uint64_t(reordered)*sc+(1ull<<45))>>46);
 bool rev=c_.direction==2||(c_.direction==1&&random()<random_threshold_);
 const uint32_t limit=1+uint32_t((uint64_t(c_.wu)*399)>>30)/100; // floor(3.99*u)
 bool repeat=false;
 if(saved_&&repeats_<limit&&random()>uint64_t(sc)*17/20*4){
  const uint64_t a=uint64_t(saved_age_)+written_-saved_at_;
  if(a<=w&&a+len+guard<uint64_t(full.frames)&&a>=ceil16(uint64_t(len)*saved_speed_)+guard){age=uint32_t(a);speed=saved_speed_;rev=saved_reverse_;++repeats_;repeat=true;}
 }
 if(!repeat){saved_=true;saved_at_=written_;saved_age_=age;saved_speed_=speed;saved_reverse_=rev;repeats_=1;}
 const uint32_t f=min(uint32_t((uint64_t(44100u)*(one+9ull*c_.gu)+one*500ull)/(one*1000ull)),len/4);
 const uint32_t duty=one-uint32_t((uint64_t(sc)*48)/100);
 const Cut cut{written_-age,len,speed,f<2?2:f,uint32_t((uint64_t(len)*duty)>>30),rev};
 start_cut(cut,fade);
}
bool Engine::start_cut(const Cut&cut,uint32_t fade){
 if(!cut.length||count_>=2||cut.oldest<written_-valid_)return false;
 // A live writer can overtake an old grid repeat before the head finishes.
 // Reserve its full lifetime within the existing ring; never read stale ink.
 if(!c_.hold&&written_+cut.length+1>cut.oldest+full.frames)return false;
 const uint32_t offset=cut.reverse?ceil16(uint64_t(cut.length-1)*cut.speed):0;
 const uint32_t max_offset=ceil16(uint64_t(cut.length-1)*cut.speed);
 if(cut.oldest+max_offset+1>=written_)return false;
 const int64_t logical=static_cast<int64_t>((cut.oldest+offset)<<16);
 const uint32_t f=fade?fade:cut.fade;
 h_[count_++]={logical,uint32_t(logical)&phase_mask,cut.speed,cut.length,cut.length,0,f,
  uint32_t(one/f),cut.active,cut.reverse?-1:1};
 ++stats_.launches;if(count_>stats_.max_heads)stats_.max_heads=count_;
 return true;
}
void Engine::tick(int32_t l,int32_t r,int32_t&ol,int32_t&orr){
 if(!ring_){ol=l;orr=r;return;}
 if(was_hold_&&!c_.hold){valid_=count_=mix_=0;saved_=false;repeats_=0;}was_hold_=c_.hold;
 if(!c_.hold&&!c_.bypass){
  store(0,l);store(1,r);write_=(write_+1)&ring_mask;++written_;
  valid_=min(full.frames,valid_+1);
 }
 if(!c_.bypass){if(!count_)launch();else if(count_==1&&h_[0].left<=h_[0].fade)launch(h_[0].left);}
 const int64_t low=static_cast<int64_t>((written_-valid_)<<16),high=static_cast<int64_t>(written_);
 int64_t wl=0,wr=0;uint32_t sum=0;
 for(int i=int(count_)-1;i>=0;--i){Head&h=h_[i];const uint32_t rem=h.active>h.age?h.active-h.age:0;
  const uint32_t edge=min(h.age,rem);
  // A launch fade is at most 441 samples. For edge <= fade the
  // Q30 product fits in 32 bits. If edge is larger, its first
  // possible product is already >= one because one/fade >= fade.
  const uint32_t gain=edge>h.fade?one:edge*h.invfade;
  if(h.logical<low||(h.logical>>16)+1>=high){++stats_.bounds_faults;} else if (gain) {
   const int32_t left = read(0, h.pos), right = read(1, h.pos);
   wl += gain == one ? left : weighted(left, gain);
   wr += gain == one ? right : weighted(right, gain);
  }
  sum+=gain;
  // Every head steps by a bounded, signed Q16 speed in both coordinates.
  const int32_t increment=h.dir*static_cast<int32_t>(h.speed);
  h.logical+=increment;h.pos=(h.pos+static_cast<uint32_t>(increment))&phase_mask;++h.age;if(!--h.left){for(unsigned k=i;k+1<count_;++k)h_[k]=h_[k+1];--count_;}
 }
 if(sum>one){++stats_.normalizations;wl=wl*one/int64_t(sum);wr=wr*one/int64_t(sum);} // audit wide helper before native use
 const uint32_t target=c_.bypass||(!count_&&!sum)?0:min(one,c_.mix),step=(one+44100u/100-1)/(44100u/100);
 if(mix_<target)mix_+=min(step,target-mix_);else mix_-=min(step,mix_-target);
 const uint32_t m=count_||sum?mix_:0;
 if (m == one) {
  ol = sat(wl); orr = sat(wr);
 } else if(m){
  ol=blend(l,static_cast<int32_t>(wl),m);
  orr=blend(r,static_cast<int32_t>(wr),m);
 }else{ol=l;orr=r;}
 if(c_.bypass&&!mix_&&!was_bypass_){valid_=count_=0;saved_=false;was_bypass_=true;}if(!c_.bypass)was_bypass_=false;
}
}
