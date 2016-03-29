#include <stdio.h>

#include <iostream>
#include <utility>

#include <sched.h>

#include <pin.H>

#include "manager.h"
#include "buffer.h"

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "data.db", "specify output file name");

KNOB<string> KnobInputFile(KNOB_MODE_WRITEONCE, "pintool",
    "i", "source.yaml", "specify source file name");

KNOB<string> KnobFilterFile(KNOB_MODE_WRITEONCE, "pintool",
    "f", "filter.yaml", "specify filter file name");

BUFFER_ID bufId;

VOID ImageLoad(IMG img, VOID *v)
{
    Manager* manager = (Manager*)v;

    INT32 column;
    INT32 line;
    string file;
    string image = IMG_Name(img);
    ADDRINT address;

    if(manager->filter.isImageFiltered(image))
        return;


    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))
    {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn))
        {
            RTN_Open(rtn);

            std::string sym = RTN_Name(rtn);

            std::string name = PIN_UndecorateSymbolName(sym, UNDECORATION_NAME_ONLY);
            std::string prototype = PIN_UndecorateSymbolName(sym, UNDECORATION_COMPLETE);

            if(manager->filter.isFunctionFiltered(name) || manager->filter.isFunctionFiltered(prototype)) {
                RTN_Close(rtn);
                continue;
            }

            address = RTN_Address(rtn);
            PIN_GetSourceLocation(address, &column, &line, &file);

            if(manager->filter.isFileFiltered(file)) {
                RTN_Close(rtn);
                continue;
            }

            int functionId = manager->writer.getFunctionIdByPrototype(prototype);

            for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
            {
                address = INS_Address(ins);
                PIN_GetSourceLocation(address, &column, &line, &file);

                SourceLocation location;

                location.function = functionId;
                location.line = line;
                location.column = column;

                if(file.length() > 0) {
                    auto it = manager->sourceLocationTagIdMap.find(location);
                    if (it != manager->sourceLocationTagIdMap.end()) {
                        INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
                             IARG_UINT32, static_cast<UINT32>(BuferEntryType::Tag), offsetof(struct BufferEntry, type),
                             IARG_INST_PTR, offsetof(struct BufferEntry, instruction),
                             IARG_TSC, offsetof(struct BufferEntry, tsc),
                             IARG_UINT32, static_cast<UINT32>(it->second), offsetof(struct BufferEntry, data) + offsetof(struct TagBufferEntry, tagId),
                             IARG_END);
                    }
                }
            }

            RTN_Close(rtn);
        }
    }

}

INT32 Usage()
{
    cerr << "This tool prints a log of image load and unload events" << endl;
    cerr << " along with static instruction counts for each image." << endl;
    cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

VOID Fini(INT32 code, VOID *v)
{
    Manager* manager = (Manager*)v;

    delete manager;
}

VOID * BufferFull(BUFFER_ID, THREADID tid, const CONTEXT *, void *buffer,
                  UINT64 n, VOID *v)
{
    Manager* manager = (Manager*)v;

    struct BufferEntry* entries = (struct BufferEntry*) buffer;

    manager->bufferFull(entries, n, tid );

    return buffer;
}

VOID ThreadStart(THREADID threadid, CONTEXT *, INT32, VOID *v)
{
    cpu_set_t my_set;
    CPU_ZERO(&my_set);
    CPU_SET(sched_getcpu(), &my_set);
    sched_setaffinity(0, sizeof(cpu_set_t), &my_set);


    Manager* manager = (Manager*)v;

    manager->setUpThreadManager(threadid);
}

VOID ThreadFini(THREADID threadid, const CONTEXT *, INT32, VOID *v)
{
    Manager* manager = (Manager*)v;

    manager->tearDownThreadManager(threadid);
}

int main(int argc, char * argv[])
{
    PIN_InitSymbols();

    Manager* manager = new Manager(KnobOutputFile.Value(), KnobInputFile.Value(), KnobFilterFile.Value());

    if (PIN_Init(argc, argv)) return Usage();

    bufId = PIN_DefineTraceBuffer(sizeof(struct BufferEntry), 100,
                BufferFull, (void*)manager);

    IMG_AddInstrumentFunction(ImageLoad, (void*)manager);
    PIN_AddFiniFunction(Fini, (void*)manager);
    PIN_AddThreadStartFunction(ThreadStart, (void*)manager);
    PIN_AddThreadFiniFunction(ThreadFini, (void*)manager);

    PIN_StartProgram();

    return 0;
}
