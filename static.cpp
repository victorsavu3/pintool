#include <stdio.h>
#include <iostream>

#include <map>
#include <memory>
#include <set>

#include <pin.H>

#include "sqlwriter.h"
#include "filter.h"

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
                            "o", "data.db", "specify output file name");


KNOB<string> KnobFilterFile(KNOB_MODE_WRITEONCE, "pintool",
                            "f", "filter.yaml", "specify filter name");

struct Manager {
    std::unique_ptr<SQLWriter> writer;
    std::unique_ptr<Filter> filter;
};

VOID ImageLoad(IMG img, VOID *v)
{
    Manager* manager = (Manager*)v;
    SQLWriter* writer = manager->writer.get();

    INT32 column;
    INT32 line;
    string file;
    ADDRINT address;

    Image image;

    image.name = IMG_Name(img);

    fprintf(stderr, "Processing %s\n", image.name.c_str());

    if(manager->filter->isImageFiltered(image.name)) {
        cerr << "Skipping image:" << image.name << endl;
        return;
    }

    std::set<SourceLocation> foundLocations;
    SourceLocation lastLocation;

    writer->insertImage(image);

    std::map<std::string, File> files;

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))
    {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn))
        {
            RTN_Open(rtn);

            std::string sym = RTN_Name(rtn);

            Function function;

            function.name = PIN_UndecorateSymbolName(sym, UNDECORATION_NAME_ONLY);
            function.prototype = PIN_UndecorateSymbolName(sym, UNDECORATION_COMPLETE);

            if(manager->filter->isFunctionFiltered(function.name) || manager->filter->isFunctionFiltered(function.prototype)) {
                cerr << "Skipping function:" << function.prototype << endl;
                RTN_Close(rtn);
                continue;
            }

            address = RTN_Address(rtn);
            PIN_GetSourceLocation(address, &column, &line, &file);

            function.line = line;

            if (files.find(file) == files.end()) {
                if (file.length() == 0) {
                    file = "Unknown";
                }

                if(manager->filter->isFileFiltered(file)) {
                    cerr << "Skipping file:" << file << endl;
                    RTN_Close(rtn);
                    continue;
                }

                files[file].name = file;
                files[file].image = image.id;

                writer->insertFile(files[file]);
            }

            function.file = files[file].id;

            function.id = writer->functionExists(function);

            if (function.id == -1)
                writer->insertFunction(function);

            for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
            {
                address = INS_Address(ins);

                PIN_GetSourceLocation(address, &column, &line, NULL);

                if(file.length() > 0) {
                    SourceLocation location;

                    location.function = function.id;
                    location.line = line;
                    location.column = column;

                    if (location != lastLocation) {
                        lastLocation = location;

                        if (foundLocations.find(location) == foundLocations.end()) {
                            writer->insertSourceLocation(location);

                            foundLocations.insert(location);
                        }
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


int main(int argc, char * argv[])
{
    // prepare for image instrumentation mode
    PIN_InitSymbols();

    Manager* manager = new Manager;

    manager->writer.reset(new SQLWriter(KnobOutputFile.Value(), true));
    manager->filter.reset(new Filter(KnobFilterFile.Value()));

    if (PIN_Init(argc, argv)) return Usage();

    IMG_AddInstrumentFunction(ImageLoad, (void*)manager);
    PIN_AddFiniFunction(Fini, (void*)manager);

    PIN_StartProgram();

    return 0;
}
