#include <iostream>

#include <pin.H>

INT32 Usage()
{
    std::cerr << "This tool pexecutes an application using Intel Pin" << std::endl;
    std::cerr << endl << KNOB_BASE::StringKnobSummary() << std::endl;
    return -1;
}


int main(int argc, char * argv[])
{
    PIN_InitSymbols();

    if (PIN_Init(argc, argv)) return Usage();

    PIN_StartProgramProbed();

    return 0;
}
