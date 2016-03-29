#ifndef ASM_H
#define ASM_H

#include <pin.H>

static inline UINT64 rdtsc(void)
{
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (UINT64)lo)|( ((UINT64)hi)<<32 );
}

#endif // ASM_H
