#pragma once
#include <stdint.h>
#include <stddef.h>
// Source-owned packed storage leaf. No Field binding, allocator, publication,
// reset, native ID, DSP callback or imported parent payload.
namespace pong_packed_access {
constexpr size_t maximum_samples=262144;
// The class body is copied exactly from the verified host packing.hpp. The
// verifier checks that source relationship and an independent bitstream model.
class View {
    uint8_t* data_;size_t count_;unsigned bits_;
public:
    View(uint8_t* data,size_t count,unsigned bits):data_(data),count_(count),bits_(bits){}
    bool put(size_t i,int32_t q){
        if(i>=count_||!data_||(bits_!=12&&bits_!=8&&bits_!=4))return false;
        if(bits_==12?(q<-2048||q>2047):(q<0||uint32_t(q)>((1u<<bits_)-1)))return false;
        const uint32_t c=uint32_t(q)&((1u<<bits_)-1);
        if(bits_==12){const size_t j=(i/2)*3;
            if(i&1){data_[j+1]=uint8_t((data_[j+1]&15)|((c&15)<<4));data_[j+2]=uint8_t(c>>4);}
            else{data_[j]=uint8_t(c);data_[j+1]=uint8_t((data_[j+1]&240)|(c>>8));}
        }else if(bits_==4){const unsigned shift=unsigned(i&1)*4;data_[i/2]=uint8_t((data_[i/2]&~(15u<<shift))|(c<<shift));}
        else data_[i]=uint8_t(c);
        return true;
    }
    bool get(size_t i,int32_t& q)const{
        if(i>=count_||!data_||(bits_!=12&&bits_!=8&&bits_!=4))return false;
        if(bits_==12){const size_t j=(i/2)*3;const uint32_t c=(i&1)?(uint32_t(data_[j+1]>>4)|(uint32_t(data_[j+2])<<4)):(uint32_t(data_[j])|((uint32_t(data_[j+1])&15)<<8));q=(c&2048)?int32_t(c)-4096:int32_t(c);}
        else q=bits_==4?(data_[i/2]>>((i&1)*4))&15:data_[i];
        return true;
    }
};
}
extern "C" {
// Extent is in bytes. The caller owns a live exclusive view; these functions
// neither establish nor change its lifetime. Odd sample counts are supported.
bool pong_packed_extent(size_t samples,unsigned bits,size_t* bytes);
bool pong_packed_write(uint8_t* data,size_t bytes,size_t samples,unsigned bits,size_t index,int32_t code);
bool pong_packed_read(const uint8_t* data,size_t bytes,size_t samples,unsigned bits,size_t index,int32_t* code);
bool pong_pcm12_from_halfword(int32_t halfword,int32_t* code);
}
