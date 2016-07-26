#include <stdio.h>

#include <iostream>
#include <utility>

#include <sched.h>

#include <pin.H>

#include "manager.h"
#include "buffer.h"
#include "exception.h"

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
                            "db", "data.db", "specify output file name");

KNOB<string> KnobInputFile(KNOB_MODE_WRITEONCE, "pintool",
                           "source", "source.yaml", "specify source file name");

KNOB<string> KnobFilterFile(KNOB_MODE_WRITEONCE, "pintool",
                            "filter", "filter.yaml", "specify filter file name");

BUFFER_ID bufId;


VOID MallocEnter(ADDRINT d, UINT64 size, UINT64 tsc, THREADID tid)
{
    Manager* manager = (Manager*)d;

    AllocEnterBufferEntry entry;

    entry.type = AllocEntryType::malloc;
    entry.size = size;
    entry.tsc = tsc;
    entry.thread = tid;

    manager->lockKnownAllocations();

    manager->knownAllocationsInProgess.insert(std::make_pair(tid, entry));

    manager->unlockKnownAllocations();
}

VOID CallocEnter(ADDRINT d, UINT64 num, UINT64 size, UINT64 tsc, THREADID tid)
{
    Manager* manager = (Manager*)d;

    AllocEnterBufferEntry entry;

    entry.type = AllocEntryType::calloc;
    entry.size = size;
    entry.num = num;
    entry.tsc = tsc;
    entry.thread = tid;

    manager->lockKnownAllocations();

    manager->knownAllocationsInProgess.insert(std::make_pair(tid, entry));

    manager->unlockKnownAllocations();
}

VOID ReallocEnter(ADDRINT d, ADDRINT ref, UINT64 size, UINT64 tsc, THREADID tid)
{
    Manager* manager = (Manager*)d;

    AllocEnterBufferEntry entry;

    entry.type = AllocEntryType::realloc;
    entry.size = size;
    entry.ref = ref;
    entry.tsc = tsc;
    entry.thread = tid;

    manager->lockKnownAllocations();

    manager->knownAllocationsInProgess.insert(std::make_pair(tid, entry));

    manager->unlockKnownAllocations();
}

VOID AllocExit(ADDRINT d, ADDRINT ref, THREADID tid)
{
    Manager* manager = (Manager*)d;

    manager->lockKnownAllocations();

    auto it = manager->knownAllocationsInProgess.find(tid);

    if (it == manager->knownAllocationsInProgess.end()) {
        manager->unlockKnownAllocations();
        return;
    }
    auto knownIt = manager->knownAllocations.find(it->second);

    if (knownIt == manager->knownAllocations.end()) {
        std::map<UINT64, ADDRINT> map;

        map.insert(std::make_pair(it->second.tsc, ref));

        manager->knownAllocations.insert(std::make_pair(it->second, map));
    } else {
        knownIt->second.insert(std::make_pair(it->second.tsc, ref));
    }

    manager->knownAllocationsInProgess.erase(it);

    manager->unlockKnownAllocations();
}

void processAlloc(Manager* manager, RTN rtn, AllocEntryType type) {

    RTN_Open(rtn);

    INS ins = RTN_InsHeadOnly(rtn);
    ADDRINT address = INS_Address(ins);

    manager->allocEnterAddresesToInstrument.insert(std::make_pair(address, type));

    switch(type) {
    case AllocEntryType::malloc:
        RTN_InsertCall( rtn, IPOINT_BEFORE, (AFUNPTR)MallocEnter, IARG_ADDRINT, (ADDRINT)manager, IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_TSC, IARG_THREAD_ID, IARG_END);
        break;
    case AllocEntryType::calloc:
        RTN_InsertCall( rtn, IPOINT_BEFORE, (AFUNPTR)CallocEnter, IARG_ADDRINT, (ADDRINT)manager, IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_FUNCARG_ENTRYPOINT_VALUE, 1, IARG_TSC, IARG_THREAD_ID, IARG_END);
        break;
    case AllocEntryType::realloc:
        RTN_InsertCall( rtn, IPOINT_BEFORE, (AFUNPTR)ReallocEnter, IARG_ADDRINT, (ADDRINT)manager, IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_FUNCARG_ENTRYPOINT_VALUE, 1, IARG_TSC, IARG_THREAD_ID, IARG_END);
        break;
    default:
        CorruptedBufferException("Invalid allocation type");
    }


    RTN_InsertCall( rtn, IPOINT_AFTER, (AFUNPTR)AllocExit, IARG_ADDRINT, (ADDRINT)manager, IARG_FUNCRET_EXITPOINT_VALUE, IARG_THREAD_ID, IARG_END);

    RTN_Close(rtn);
}

VOID ImageLoad(IMG img, VOID *v)
{
    Manager* manager = (Manager*)v;

    manager->lock();
    PIN_LockClient();

    RTN freeRtn = RTN_FindByName(img, "free");
    if (RTN_Valid(freeRtn))
    {
        RTN_Open(freeRtn);

        INS ins = RTN_InsHeadOnly(freeRtn);
        ADDRINT address = INS_Address(ins);

        manager->freeEnterAddresesToInstrument.insert(address);

        RTN_Close(freeRtn);
    }

    RTN mallocRtn = RTN_FindByName(img, "malloc");
    if (RTN_Valid(mallocRtn))
    {
        processAlloc(manager, mallocRtn, AllocEntryType::malloc);
    }

    RTN callocRtn = RTN_FindByName(img, "calloc");
    if (RTN_Valid(callocRtn))
    {
        processAlloc(manager, callocRtn, AllocEntryType::calloc);
    }

    RTN reallocRtn = RTN_FindByName(img, "realloc");
    if (RTN_Valid(reallocRtn))
    {
        processAlloc(manager, reallocRtn, AllocEntryType::realloc);
    }

    INT32 column;
    INT32 line;
    string file;
    string image = IMG_Name(img);
    ADDRINT address;

    if(!manager->filter.isImageFiltered(image))
    {
        int imageId = manager->writer.getImageIdByName(image);

        for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))
        {
            for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn))
            {
                RTN_Open(rtn);

                std::string sym = RTN_Name(rtn);

                std::string name = PIN_UndecorateSymbolName(sym, UNDECORATION_NAME_ONLY);
                std::string prototype = PIN_UndecorateSymbolName(sym, UNDECORATION_COMPLETE);

                if(manager->filter.isFunctionFiltered(name) || manager->filter.isFunctionFiltered(prototype))
                {
                    RTN_Close(rtn);
                    continue;
                }

                address = RTN_Address(rtn);
                PIN_GetSourceLocation(address, &column, &line, &file);

                if(manager->filter.isFileFiltered(file) || (file=="") && manager->filter.isFileFiltered("Unknown"))
                {
                    RTN_Close(rtn);
                    continue;
                }

                int functionId = manager->writer.getFunctionIdByProperties(prototype, imageId, file, line);

                INS ins = RTN_InsHeadOnly(rtn);
                address = INS_Address(ins);

                manager->callAddressesToInstrument.insert(std::make_pair(address, (CallEnterBufferEntry)
                {
                    (UINT32)functionId
                }));

                bool needsSourceScan = false;
                for (auto it : manager->sourceLocationTagInstructionIdMap)
                {
                    if(it.first.function == functionId)
                    {
                        needsSourceScan = true;
                        break;
                    }
                }

                if (needsSourceScan)
                {
                    for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
                    {
                        address = INS_Address(ins);
                        PIN_GetSourceLocation(address, &column, &line, NULL);

                        SourceLocation location;

                        location.function = functionId;
                        location.line = line;
                        location.column = column;

                        if(file.length() > 0)
                        {
                            auto it = manager->sourceLocationTagInstructionIdMap.find(location);
                            if (it != manager->sourceLocationTagInstructionIdMap.end())
                            {
                                manager->tagAddressesToInstrument.insert(std::make_pair(address, (TagBufferEntry)
                                {
                                    (UINT32)it->second
                                }));
                            }
                        }
                    }
                }

                for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
                {
                    ADDRINT address = INS_Address(ins);

                    if(INS_IsRet(ins))
                    {
                        manager->retAddressesToInstrument.insert(std::make_pair(address, (RetBufferEntry)
                        {
                            (UINT32)functionId
                        }));
                    }

                    if (INS_IsStandardMemop(ins) || INS_HasMemoryVector(ins))
                    {
                        UINT32 memoryOperandCount = INS_MemoryOperandCount(ins);

                        if (memoryOperandCount == 0)
                            continue;

                        AccessInstructionDetails entry;
                        entry.accesses.reserve(memoryOperandCount);
                        entry.location = manager->getLocation(address, functionId);

                        for (UINT32 memOp = 0; memOp < memoryOperandCount; memOp++)
                        {
                            MemoryOperationDetails opDetail;

                            opDetail.size = INS_MemoryOperandSize(ins, memOp);
                            opDetail.isRead = INS_MemoryOperandIsRead(ins, memOp);

                            entry.accesses.push_back(opDetail);
                        }

                        manager->accessDetails.push_back(entry);

                        manager->accessToInstrument.insert(std::make_pair(address, manager->accessDetails.size() - 1));
                    }

                    if (INS_IsCall(ins))
                    {
                        ADDRINT detail = (ADDRINT)manager->getLocation(address, functionId);

                        manager->callInstructionAddressesToInstrument.insert(std::make_pair(address, detail));
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
        for(INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins=INS_Next(ins))
        {
            ADDRINT address = INS_Address(ins);

            {
                auto it = manager->freeEnterAddresesToInstrument.find(address);

                if (it != manager->freeEnterAddresesToInstrument.end())
                {
                    INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
                                         IARG_UINT32, static_cast<UINT32>(BuferEntryType::Free), offsetof(struct BufferEntry, type),
                                         IARG_FUNCARG_ENTRYPOINT_VALUE, 0, offsetof(struct BufferEntry, data) + offsetof(struct FreeBufferEntry, ref),
                                         IARG_END);

                    continue;
                }
            }

            {
                auto it = manager->allocEnterAddresesToInstrument.find(address);

                if (it != manager->allocEnterAddresesToInstrument.end())
                {
                    switch(it->second) {
                    case AllocEntryType::malloc:
                        INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
                                             IARG_UINT32, static_cast<UINT32>(BuferEntryType::AllocEnter), offsetof(struct BufferEntry, type),
                                             IARG_UINT32, static_cast<UINT32>(it->second), offsetof(struct BufferEntry, data) + offsetof(struct AllocEnterBufferEntry, type),
                                             IARG_TSC, offsetof(struct BufferEntry, data) + offsetof(struct AllocEnterBufferEntry, tsc),
                                             IARG_THREAD_ID, offsetof(struct BufferEntry, data) + offsetof(struct AllocEnterBufferEntry, thread),
                                             IARG_FUNCARG_ENTRYPOINT_VALUE, 0, offsetof(struct BufferEntry, data) + offsetof(struct AllocEnterBufferEntry, size),
                                             IARG_END);
                        break;
                    case AllocEntryType::calloc:
                        INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
                                             IARG_UINT32, static_cast<UINT32>(BuferEntryType::AllocEnter), offsetof(struct BufferEntry, type),
                                             IARG_UINT32, static_cast<UINT32>(it->second), offsetof(struct BufferEntry, data) + offsetof(struct AllocEnterBufferEntry, type),
                                             IARG_TSC, offsetof(struct BufferEntry, data) + offsetof(struct AllocEnterBufferEntry, tsc),
                                             IARG_THREAD_ID, offsetof(struct BufferEntry, data) + offsetof(struct AllocEnterBufferEntry, thread),
                                             IARG_FUNCARG_ENTRYPOINT_VALUE, 0, offsetof(struct BufferEntry, data) + offsetof(struct AllocEnterBufferEntry, num),
                                             IARG_FUNCARG_ENTRYPOINT_VALUE, 1, offsetof(struct BufferEntry, data) + offsetof(struct AllocEnterBufferEntry, size),
                                             IARG_END);
                        break;
                    case AllocEntryType::realloc:
                        INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
                                             IARG_UINT32, static_cast<UINT32>(BuferEntryType::AllocEnter), offsetof(struct BufferEntry, type),
                                             IARG_UINT32, static_cast<UINT32>(it->second), offsetof(struct BufferEntry, data) + offsetof(struct AllocEnterBufferEntry, type),
                                             IARG_TSC, offsetof(struct BufferEntry, data) + offsetof(struct AllocEnterBufferEntry, tsc),
                                             IARG_THREAD_ID, offsetof(struct BufferEntry, data) + offsetof(struct AllocEnterBufferEntry, thread),
                                             IARG_FUNCARG_ENTRYPOINT_VALUE, 0, offsetof(struct BufferEntry, data) + offsetof(struct AllocEnterBufferEntry, ref),
                                             IARG_FUNCARG_ENTRYPOINT_VALUE, 1, offsetof(struct BufferEntry, data) + offsetof(struct AllocEnterBufferEntry, size),
                                             IARG_END);
                        break;
                    default:
                        UnimplementedException("Unknown allocator type");
                    }

                    continue;
                }
            }

            {
                auto it = manager->tagAddressesToInstrument.find(address);

                if (it != manager->tagAddressesToInstrument.end())
                {
                    INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
                                         IARG_UINT32, static_cast<UINT32>(BuferEntryType::Tag), offsetof(struct BufferEntry, type),
                                         IARG_TSC, offsetof(struct BufferEntry, data) + offsetof(struct TagBufferEntry, tsc),
                                         IARG_UINT32, static_cast<UINT32>(it->second.tagId), offsetof(struct BufferEntry, data) + offsetof(struct TagBufferEntry, tagId),
                                         IARG_INST_PTR, offsetof(struct BufferEntry, data) + offsetof(struct TagBufferEntry, address),
                                         IARG_END);
                }
            }

            {
                auto it = manager->callInstructionAddressesToInstrument.find(address);

                if (it != manager->callInstructionAddressesToInstrument.end())
                {
                    INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
                                         IARG_UINT32, static_cast<UINT32>(BuferEntryType::Call), offsetof(struct BufferEntry, type),
                                         IARG_TSC, offsetof(struct BufferEntry, data) + offsetof(struct CallInstructionBufferEntry, tsc),
                                         IARG_REG_VALUE, REG_RSP, offsetof(struct BufferEntry, data) + offsetof(struct CallInstructionBufferEntry, rsp),
                                         IARG_UINT32, it->second, offsetof(struct BufferEntry, data) + offsetof(struct CallInstructionBufferEntry, location),
                                         IARG_END);
                }
            }

            {
                auto it = manager->callAddressesToInstrument.find(address);

                if (it != manager->callAddressesToInstrument.end())
                {
                    INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
                                         IARG_UINT32, static_cast<UINT32>(BuferEntryType::CallEnter), offsetof(struct BufferEntry, type),
                                         IARG_UINT32, static_cast<UINT32>(it->second.functionId), offsetof(struct BufferEntry, data) + offsetof(struct CallEnterBufferEntry, functionId),
                                         IARG_TSC, offsetof(struct BufferEntry, data) + offsetof(struct CallEnterBufferEntry, tsc),
                                         IARG_REG_VALUE, REG_GBP, offsetof(struct BufferEntry, data) + offsetof(struct CallEnterBufferEntry, rbp),
                                         IARG_REG_VALUE, REG_RSP, offsetof(struct BufferEntry, data) + offsetof(struct CallEnterBufferEntry, rsp),
                                         IARG_END);
                }
            }

            {
                auto it = manager->retAddressesToInstrument.find(address);

                if (it != manager->retAddressesToInstrument.end())
                {
                    INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
                                         IARG_UINT32, static_cast<UINT32>(BuferEntryType::Ret), offsetof(struct BufferEntry, type),
                                         IARG_TSC, offsetof(struct BufferEntry, data) + offsetof(struct RetBufferEntry, tsc),
                                         IARG_REG_VALUE, REG_RSP, offsetof(struct BufferEntry, data) + offsetof(struct RetBufferEntry, rsp),
                                         IARG_UINT32, static_cast<UINT32>(it->second.functionId), offsetof(struct BufferEntry, data) + offsetof(struct RetBufferEntry, functionId),
                                         IARG_END);
                }
            }

            {
                auto it = manager->accessToInstrument.find(address);

                if (it != manager->accessToInstrument.end())
                {
                    AccessInstructionDetails& detail = manager->accessDetails[it->second];
                    AccessInstructionDetails* detailPtr = &detail;

                    switch(detail.accesses.size())
                    {
                    case 1:
                        INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
                                             IARG_UINT32, static_cast<UINT32>(BuferEntryType::MemRef), offsetof(struct BufferEntry, type),
                                             IARG_REG_VALUE, REG_RSP, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, rsp),
                                             IARG_ADDRINT, (ADDRINT)detailPtr, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, accessDetails),
                                             IARG_MEMORYOP_EA, 0, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 0 * sizeof(ADDRINT),
                                             IARG_END);
                        break;
                    case 2:
                        INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
                                             IARG_UINT32, static_cast<UINT32>(BuferEntryType::MemRef), offsetof(struct BufferEntry, type),
                                             IARG_REG_VALUE, REG_RSP, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, rsp),
                                             IARG_ADDRINT, (ADDRINT)detailPtr, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, accessDetails),
                                             IARG_MEMORYOP_EA, 0, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 0 * sizeof(ADDRINT),
                                             IARG_MEMORYOP_EA, 1, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 1 * sizeof(ADDRINT),
                                             IARG_END);
                        break;
                    case 3:
                        INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
                                             IARG_UINT32, static_cast<UINT32>(BuferEntryType::MemRef), offsetof(struct BufferEntry, type),
                                             IARG_REG_VALUE, REG_RSP, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, rsp),
                                             IARG_ADDRINT, (ADDRINT)detailPtr, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, accessDetails),
                                             IARG_MEMORYOP_EA, 0, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 0 * sizeof(ADDRINT),
                                             IARG_MEMORYOP_EA, 1, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 1 * sizeof(ADDRINT),
                                             IARG_MEMORYOP_EA, 2, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 2 * sizeof(ADDRINT),
                                             IARG_END);
                        break;
                    case 4:
                        INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
                                             IARG_UINT32, static_cast<UINT32>(BuferEntryType::MemRef), offsetof(struct BufferEntry, type),
                                             IARG_REG_VALUE, REG_RSP, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, rsp),
                                             IARG_ADDRINT, (ADDRINT)detailPtr, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, accessDetails),
                                             IARG_MEMORYOP_EA, 0, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 0 * sizeof(ADDRINT),
                                             IARG_MEMORYOP_EA, 1, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 1 * sizeof(ADDRINT),
                                             IARG_MEMORYOP_EA, 2, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 2 * sizeof(ADDRINT),
                                             IARG_MEMORYOP_EA, 3, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 3 * sizeof(ADDRINT),
                                             IARG_END);
                        break;
                    case 5:
                        INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
                                             IARG_UINT32, static_cast<UINT32>(BuferEntryType::MemRef), offsetof(struct BufferEntry, type),
                                             IARG_REG_VALUE, REG_RSP, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, rsp),
                                             IARG_ADDRINT, (ADDRINT)detailPtr, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, accessDetails),
                                             IARG_MEMORYOP_EA, 0, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 0 * sizeof(ADDRINT),
                                             IARG_MEMORYOP_EA, 1, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 1 * sizeof(ADDRINT),
                                             IARG_MEMORYOP_EA, 2, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 2 * sizeof(ADDRINT),
                                             IARG_MEMORYOP_EA, 3, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 3 * sizeof(ADDRINT),
                                             IARG_MEMORYOP_EA, 4, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 4 * sizeof(ADDRINT),
                                             IARG_END);
                        break;
                    case 6:
                        INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
                                             IARG_UINT32, static_cast<UINT32>(BuferEntryType::MemRef), offsetof(struct BufferEntry, type),
                                             IARG_REG_VALUE, REG_RSP, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, rsp),
                                             IARG_ADDRINT, (ADDRINT)detailPtr, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, accessDetails),
                                             IARG_MEMORYOP_EA, 0, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 0 * sizeof(ADDRINT),
                                             IARG_MEMORYOP_EA, 1, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 1 * sizeof(ADDRINT),
                                             IARG_MEMORYOP_EA, 2, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 2 * sizeof(ADDRINT),
                                             IARG_MEMORYOP_EA, 3, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 3 * sizeof(ADDRINT),
                                             IARG_MEMORYOP_EA, 4, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 4 * sizeof(ADDRINT),
                                             IARG_MEMORYOP_EA, 5, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 5 * sizeof(ADDRINT),
                                             IARG_END);
                        break;
                    case 7:
                        INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
                                             IARG_UINT32, static_cast<UINT32>(BuferEntryType::MemRef), offsetof(struct BufferEntry, type),
                                             IARG_REG_VALUE, REG_RSP, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, rsp),
                                             IARG_ADDRINT, (ADDRINT)detailPtr, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, accessDetails),
                                             IARG_MEMORYOP_EA, 0, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 0 * sizeof(ADDRINT),
                                             IARG_MEMORYOP_EA, 1, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 1 * sizeof(ADDRINT),
                                             IARG_MEMORYOP_EA, 2, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 2 * sizeof(ADDRINT),
                                             IARG_MEMORYOP_EA, 3, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 3 * sizeof(ADDRINT),
                                             IARG_MEMORYOP_EA, 4, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 4 * sizeof(ADDRINT),
                                             IARG_MEMORYOP_EA, 5, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 5 * sizeof(ADDRINT),
                                             IARG_MEMORYOP_EA, 6, offsetof(struct BufferEntry, data) + offsetof(struct AccessInstructionBufferEntry, addresses) + 6 * sizeof(ADDRINT),
                                             IARG_END);
                        break;
                    default:
                        UnimplementedException("Too many memory operations per instruction");
                        break;
                    }
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

    return buffer;
}

void bindThreadToCore()
{
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

    if (PIN_Init(argc, argv)) return Usage();

    Manager* manager = new Manager(KnobOutputFile.Value(), KnobInputFile.Value(), KnobFilterFile.Value());

    bufId = PIN_DefineTraceBuffer(sizeof(struct BufferEntry), 100000,
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
