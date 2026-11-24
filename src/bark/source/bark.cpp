#include "build_r6e_card.h"
#include "portable/bark_families.hpp"

struct BarkVoice {
    bark_families::Voice kernel;
    unsigned voice_style;
    unsigned reserved[129]; // Preserve the published voice reservation and state layout.
};
static_assert(sizeof(BarkVoice)==560, "keep the card reservation in sync");
volatile unsigned bark_notes[8];

extern "C" void bark_render(const Op1fVoiceBlock* block) OP1F_NAMED("bark_render");
extern "C" void bark_render(const Op1fVoiceBlock* block) {
    int* out=block->out;
    auto* state=static_cast<BarkVoice*>(block->state);
    if(block->bytes<52 || !state || block->frames!=128 || !block->values) {
        for(unsigned i=0;i<2*block->frames;++i) out[i]=0;
        return;
    }
    if(block->events&OP1F_VOICE_FRESH) {
        state->kernel.active=0;
        state->kernel.wag=0;
    }
    if(block->events&OP1F_VOICE_BEGINS) {
        bark_families::start(state->kernel); // Match the native Bark excitation, with stock velocity downstream.
        state->voice_style=(unsigned(block->values[4])>>13) | (bark_contact::decode(block->values[6])<<2);
        if(block->slot<8) ++bark_notes[block->slot];
    }
    if(!state->kernel.active) {
        for(unsigned i=0;i<256;++i) out[i]=0;
        return;
    }
    const int* v=block->values;
    const int words[4]={v[0]*65536,v[1]*65536,v[2]*65536,v[3]*65536};
    bark_families::render(state->kernel,out,block->step,words,state->voice_style&3,state->voice_style>>2);
}
