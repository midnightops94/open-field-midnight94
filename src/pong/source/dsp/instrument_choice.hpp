#pragma once
#include <stdint.h>
// Source-only tentative canonical word: extends the prior 12-code mapper by
// adding wall as the major dimension. Not the inherited f329 48-code format;
// no native producer, saved-data migration, or public wire ABI is claimed.
namespace pong_instrument_choice {
struct Choice {unsigned bits; bool sync,parallel; unsigned wall;};
// Internal owner word only. The stock preset parameter format is separate.
inline bool popup_event(uint32_t event){
 return event>=0x36u&&event<=0x39u;
}
inline unsigned next_texture(unsigned bits){return bits==12u?8u:bits==8u?4u:12u;}
inline unsigned previous_texture(unsigned bits){return bits==12u?4u:bits==8u?12u:8u;}
inline bool decode(uint32_t word,Choice& out){
 const unsigned n=word>>16;
 if((word&0xffffu)!=0 || n>=36u)return false;
 unsigned w=0, within=n;
 if(within>=24u){w=2;within-=24u;}
 else if(within>=12u){w=1;within-=12u;}
 unsigned t=within>=8u?2u:within>=4u?1u:0u;
 Choice c{t==0?12u:t==1?8u:4u,bool(n&1u),bool(n&2u),w==0?2u:w==1?0u:1u};
 out=c;return true;
}
inline bool encode(Choice c,uint32_t& out){
 if(c.wall>2)return false;
 const unsigned t=c.bits==12?0u:c.bits==8?1u:c.bits==4?2u:3u;
 if(t==3u)return false;
 unsigned w=c.wall==2?0u:c.wall==0?1u:2u;
 out=(w*12u+t*4u+2u*unsigned(c.parallel)+unsigned(c.sync))<<16;return true;
}
// Pure proposal only; caller must prove selected bank ownership and commit.
inline bool propose(uint32_t word,unsigned event,uint32_t& out){
 Choice c{};if(!decode(word,c))return false;
 switch(event){
 case 0x36: c.sync=!c.sync;break;
 case 0x37: c.wall=c.wall==2u?0u:c.wall==0u?1u:2u;break;
 case 0x38: c.parallel=!c.parallel;break;
 case 0x39: c.bits=next_texture(c.bits);break;
 default:return false;
 }
 return encode(c,out);
}

}
