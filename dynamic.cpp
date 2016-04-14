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

void printEvent(UINT32 functionId, char* event) {
    std::cerr << "Dynamic: " << functionId << ": " << event << std::endl;
}

void printBufferEvent(UINT32 functionId, char* event) {
    std::cerr << "Buffer: " << functionId << ": " << event << std::endl;
}

VOID ImageLoad(IMG img, VOID *v)
{
    Manager* manager = (Manager*)v;

    manager->lock();
    PIN_LockClient();

    INT32 column;
    INT32 line;
    string file;
    string image = IMG_Name(img);
    ADDRINT address;

    if(!manager->filter.isImageFiltered(image)) {
        int imageId = manager->writer.getImageIdByName(image);

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

                if(manager->filter.isFileFiltered(file) || (file=="") && manager->filter.isFileFiltered("Unknown")) {
                    RTN_Close(rtn);
                    continue;
                }

                int functionId = manager->writer.getFunctionIdByProperties(prototype, imageId, file, line);

                INS ins = RTN_InsHeadOnly(rtn);
                address = INS_Address(ins);

                manager->callAddressesToInstrument.insert(std::make_pair(address, (CallBufferEntry){(UINT32)functionId}));
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)printEvent, IARG_UINT32, (UINT32)functionId, IARG_PTR, "enter", IARG_END);

                bool needsSourceScan = false;
                for (auto it : manager->sourceLocationTagInstructionIdMap) {
                    if(it.first.function == functionId) {
                        needsSourceScan = true;
                        break;
                    }
                }

                if (needsSourceScan) {
                    for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
                    {
                        address = INS_Address(ins);
                        PIN_GetSourceLocation(address, &column, &line, &file);

                        SourceLocation location;

                        location.function = functionId;
                        location.line = line;
                        location.column = column;

                        if(file.length() > 0) {
                            auto it = manager->sourceLocationTagInstructionIdMap.find(location);
                            if (it != manager->sourceLocationTagInstructionIdMap.end()) {
                                manager->tagAddressesToInstrument.insert(std::make_pair(address, (TagBufferEntry){(UINT32)it->second}));
                            }
                        }
                    }
                }

                for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
                {
                    if(INS_IsRet(ins)) {
                        address = INS_Address(ins);
                        manager->retAddressesToInstrument.insert(std::make_pair(address, (RetBufferEntry){(UINT32)functionId}));
                    }
                }

                RTN_Close(rtn);
            }
        }
    }
    manager->unlock();
    PIN_UnlockClient();
}

VOID Trace(TRACE trace, VOID *v)
{
    Manager* manager = (Manager*)v;

    manager->lock();
    PIN_LockClient();

    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        for(INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins=INS_Next(ins)) {
            ADDRINT address = INS_Address(ins);

            {
                auto it = manager->tagAddressesToInstrument.find(address);

                if (it != manager->tagAddressesToInstrument.end()) {
                    INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
                         IARG_UINT32, static_cast<UINT32>(BuferEntryType::Tag), offsetof(struct BufferEntry, type),
                         IARG_INST_PTR, offsetof(struct BufferEntry, instruction),
                         IARG_TSC, offsetof(struct BufferEntry, tsc),
                         IARG_UINT32, static_cast<UINT32>(it->second.tagId), offsetof(struct BufferEntry, data) + offsetof(struct TagBufferEntry, tagId),
                         IARG_END);
                }
            }

            {
                auto it = manager->callAddressesToInstrument.find(address);

                if (it != manager->callAddressesToInstrument.end()) {
                    INS_InsertFillBufferPredicated(ins, IPOINT_BEFORE, bufId,
                         IARG_UINT32, static_cast<UINT32>(BuferEntryType::Call), offsetof(struct BufferEntry, type),
                         IARG_INST_PTR, offsetof(struct BufferEntry, instruction),
                         IARG_TSC, offsetof(struct BufferEntry, tsc),
                         IARG_UINT32, static_cast<UINT32>(it->second.functionId), offsetof(struct BufferEntry, data) + offsetof(struct CallBufferEntry, functionId),
                         IARG_END);
                }
            }

            {
                auto it = manager->retAddressesToInstrument.find(address);

                if (it != manager->retAddressesToInstrument.end()) {
                    INS_InsertFillBufferPredicated(ins, IPOINT_BEFORE, bufId,
                         IARG_UINT32, static_cast<UINT32>(BuferEntryType::Ret), offsetof(struct BufferEntry, type),
                         IARG_INST_PTR, offsetof(struct BufferEntry, instruction),
                         IARG_TSC, offsetof(struct BufferEntry, tsc),
                         IARG_UINT32, static_cast<UINT32>(it->second.functionId), offsetof(struct BufferEntry, data) + offsetof(struct RetBufferEntry, functionId),
                         IARG_END);

                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)printEvent, IARG_UINT32, static_cast<UINT32>(it->second.functionId), IARG_PTR, "return", IARG_END);
                }
            }
        }
    }

    manager->unlock();
    PIN_UnlockClient();
}

INT32 Usage()
{
    cerr << KNOB_BASE::StringKnobSummary() << endl;
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

    for(UINT32 i = 0; i < n; i++)
        switch(entries[i].type) {
            case BuferEntryType::Call:
                printBufferEvent(entries[i].data.call.functionId, "Call");
                break;
            case BuferEntryType::Ret:
                printBufferEvent(entries[i].data.ret.functionId, "Return");
                break;
        }

    return buffer;
}

void bindThreadToCore() {
    cpu_set_t my_set;
    CPU_ZERO(&my_set);
    CPU_SET(sched_getcpu(), &my_set);
    sched_setaffinity(0, sizeof(cpu_set_t), &my_set);
}

VOID ThreadStart(THREADID threadid, CONTEXT *, INT32, VOID *v)
{
    bindThreadToCore();

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

    bufId = PIN_DefineTraceBuffer(sizeof(struct BufferEntry), 5,
                BufferFull, (void*)manager);

    if(bufId == BUFFER_ID_INVALID)
    {
        std::cerr << "Error: could not allocate initial buffer" << endl;
        return 1;
    }

    IMG_AddInstrumentFunction(ImageLoad, (void*)manager);
    PIN_AddFiniFunction(Fini, (void*)manager);
    PIN_AddThreadStartFunction(ThreadStart, (void*)manager);
    PIN_AddThreadFiniFunction(ThreadFini, (void*)manager);
    TRACE_AddInstrumentFunction(Trace, (void*)manager);

    PIN_StartProgram();

    return 0;
}
