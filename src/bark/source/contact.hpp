#pragma once
namespace bark_contact {
enum Kind { Original, Felt, Rubber, Wood };
// Keep every value of the v1 layout word (six) untouched. Word seven held 32767
// in all eight authored patches and had no published meaning in Bark v1 or v2.
// Narrow interior recall windows also tolerate the base's carrier quantization.
constexpr int words[]={0,8192,16384,24576};
inline unsigned decode(int word){
    for(unsigned i=1;i<4;++i)if(unsigned(word-words[i]+32)<=64)return i;
    return Original;
}
}
