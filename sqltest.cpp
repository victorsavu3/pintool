#include <stdio.h>
#include <iostream>
#include <fstream>

#include <map>
#include <memory>
#include <set>

#include <pin.H>

#include "sqlwriter.h"
#include "asm.h"

int main(int argc, char * argv[])
{
    PIN_Init(argc, argv);

    std::vector<Call> calls;

    calls.resize(10000000);

    for(auto&it : calls) {
        it.genId();

        it.function = rand();
        it.thread = rand();
        it.instruction = rand();
        it.start = rand();
        it.end = rand();
    }

    UINT64 startSQL = rdtsc();

    SQLWriter writer("test.db", true);

    for(auto&it : calls) {
        writer.insertCall(it);
    }

    UINT64 endSQL = rdtsc();

    UINT64 startText = rdtsc();

    std::ofstream out("test.txt");

    for(auto&it : calls) {
        out << it.id << ' ' << it.function << ' ' << it.thread << ' ' << it.instruction << ' ' << it.start << ' ' << it.end << std::endl;
    }

    UINT64 endText = rdtsc();

    double diff = (double)(endText - startText) / (double)(endSQL - startSQL);

    std::cout << diff << std::endl;

    PIN_StartProgram();

    return 0;
}
