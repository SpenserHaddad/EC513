#include <iostream>
#include <fstream>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include "pin.H"

static UINT64 alignedMemoryAccesses = 0;
static UINT64 unalignedMemoryAccesses = 0;

//Cache analysis routine
void cacheStore(UINT32 virtualAddr)
{
    // Check if memory alignment ends with b'00
    if (virtualAddr & 3 == 0)
        alignedMemoryAccesses++;
    else
        unalignedMemoryAccesses++;
}

// This knob will set the outfile name
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
			    "o", "results.out", "specify optional output file name");

// Pin calls this function every time a new instruction is encountered
VOID Instruction(INS ins, VOID *v)
{
    if(INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)addressAnalysis, IARG_MEMORYWRITE_EA, IARG_END);
}

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
    ofstream outfile;
    outfile.open(KnobOutputFile.Value().c_str());
    outfile.setf(ios::showbase);
    outfile << "aligned memory accesses: " << alignedMemoryAccesses;
    outfile << "unaligned memory accesses: " << unalignedMemoryAccesses;

    outfile.close();
}

// argc, argv are the entire command line, including pin -t <toolname> -- ...
int main(int argc, char * argv[])
{
    // Initialize pin
    PIN_Init(argc, argv);

    // Register Instruction to be called to instrument instructions
    INS_AddInstrumentFunction(Instruction, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}