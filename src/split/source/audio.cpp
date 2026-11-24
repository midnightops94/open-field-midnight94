#include "effect.hpp"
#include "kernel.hpp"
#if defined(__bfin__) || defined(__BFIN__)
extern "C" void* memset(void* dest,int value,unsigned length) {
    volatile unsigned char* bytes=static_cast<unsigned char*>(dest);
    while(length--)*bytes++=static_cast<unsigned char>(value);
    return dest;
}
#endif
namespace {
using split_audio::Kernel;
struct State{Kernel kernel;unsigned magic,mode;int previous[4];};
static_assert(sizeof(State)<=256,"effect instance state exceeds declared memory");
}
extern "C" void split_reset(unsigned,void*) OP1F_NAMED("split_reset");
extern "C" void split_reset(unsigned,void* data){if(data)static_cast<State*>(data)->magic=0;}
extern "C" void split_process(const Op1fEffectBlock*) OP1F_NAMED("split_process");
extern "C" void split_process(const Op1fEffectBlock* b){
    if(!b || b->bytes<44 || b->frames!=64 || !b->left || !b->right || !b->values || !b->state || !b->on)return;
    State& state=*static_cast<State*>(b->state);
    unsigned mode=0;
    for(unsigned lane=0;lane<4;++lane)mode|=split_mode(b->place,lane)<<(2*lane);
    bool fresh=state.magic!=0x46583031u || b->fresh;
    bool changed=fresh || mode!=state.mode;
    for(unsigned i=0;i<4;++i)changed|=b->values[i]!=state.previous[i];
    if(changed){
        if(fresh){state.kernel.initialize(b->values,mode);state.magic=0x46583031u;}
        else state.kernel.targets(b->values,mode);
        state.mode=mode;
        for(unsigned i=0;i<4;++i)state.previous[i]=b->values[i];
    }
    state.kernel.process(b->left,b->right,64);
    split_publish_meter(b->place,0,fixed::clamp(state.kernel.level>>9,0,32767));
}
