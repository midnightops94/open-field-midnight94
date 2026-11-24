#include <stddef.h>
// Owned freestanding implementations of the standard C byte-copy/fill calls
// emitted by the exact compiler for aggregate operations. No firmware symbol
// is bound. The copy precondition is non-overlapping valid spans. The target
// object audit rejects recursion/imports. Host tests rename only the symbols
// to avoid replacing the sanitizer/runtime's own C functions.
#ifdef PONG_HOST_RUNTIME_TEST
#define PONG_COPY pong_owned_memcpy
#define PONG_FILL pong_owned_memset
#else
#define PONG_COPY memcpy
#define PONG_FILL memset
#endif
extern "C" void* PONG_COPY(void* destination,const void* source,size_t count){
    auto* d=static_cast<unsigned char*>(destination);const auto* s=static_cast<const unsigned char*>(source);
    for(size_t n=0;n<count;n++)d[n]=s[n];
    return destination;
}
extern "C" void* PONG_FILL(void* destination,int value,size_t count){
    auto* d=static_cast<unsigned char*>(destination);
    for(size_t n=0;n<count;n++)d[n]=static_cast<unsigned char>(value);
    return destination;
}
