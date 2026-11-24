#include "packed_access.hpp"
extern "C" bool pong_packed_extent(size_t count,unsigned bits,size_t* bytes){
    if(!bytes||!count||count>pong_packed_access::maximum_samples||(bits!=12&&bits!=8&&bits!=4))return false;
    *bytes=(count*bits+7u)/8u;return true;
}
extern "C" bool pong_packed_write(uint8_t* data,size_t bytes,size_t count,unsigned bits,size_t index,int32_t code){
    size_t need=0;if(!data||!pong_packed_extent(count,bits,&need)||bytes<need)return false;
    pong_packed_access::View view(data,count,bits);return view.put(index,code);
}
extern "C" bool pong_packed_read(const uint8_t* data,size_t bytes,size_t count,unsigned bits,size_t index,int32_t* code){
    size_t need=0;if(!data||!code||!pong_packed_extent(count,bits,&need)||bytes<need)return false;
    // View::get only reads; retaining its exact source body avoids a second
    // packing implementation. No borrowed pointer is retained after return.
    pong_packed_access::View view(const_cast<uint8_t*>(data),count,bits);return view.get(index,*code);
}
extern "C" bool pong_pcm12_from_halfword(int32_t h,int32_t* code){
    if(!code||h< -32768||h>32767)return false;
    *code=h/16;return true; // Exact signed truncation toward zero; no float.
}
