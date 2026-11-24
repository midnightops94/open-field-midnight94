#include "codec.hpp"
#include "tables.hpp"
extern "C" bool pong_encode_halfword(int32_t h,unsigned bits,int32_t* out){
    if(!out||h< -32768||h>32767)return false;
    if(bits==12){*out=h/16;return true;}
    if(bits!=8&&bits!=4)return false;
    const uint16_t* thresholds=bits==8?pong_codec_tables::threshold8:pong_codec_tables::threshold4;
    const unsigned sign=bits==8?128u:8u;
    const uint32_t magnitude=uint32_t(h<0?-h:h);
    // Upper-bound search, including the attainable -32768 endpoint. The
    // sign bit is retained for negative inputs quantized to magnitude zero.
    unsigned low=0,high=sign;
    while(low+1<high){const unsigned mid=(low+high)/2;if(magnitude>=thresholds[mid])low=mid;else high=mid;}
    *out=int32_t(low|(h<0?sign:0u));return true;
}
extern "C" bool pong_decode_q31(int32_t q,unsigned bits,int32_t* out){
    if(!out)return false;
    if(bits==12){if(q< -2048||q>2047)return false;*out=q*1048576;return true;}
    if(bits!=8&&bits!=4)return false;
    if(q<0||uint32_t(q)>((1u<<bits)-1u))return false;
    *out=bits==8?pong_codec_tables::decode8[q]:pong_codec_tables::decode4[q];return true;
}
