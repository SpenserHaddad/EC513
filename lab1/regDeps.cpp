#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <assert.h>
#include <map>
#include <vector>
#include <set>
#include "pin.H"

ofstream OutFile;

// Set true to treat partial registers as full registers, per lab requirements
// Set false to look at each register individually, for question 3.
static bool USE_FULL_REGISTERS_ONLY = true;

// The array storing the spacing frequency between two dependant instructions
UINT64 *dependancySpacing;

// Output file name
INT32 maxSize;

// This knob sets the output file name
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "result.csv", "specify the output file name");

// This knob will set the maximum spacing between two dependant instructions in the program
KNOB<string> KnobMaxSpacing(KNOB_MODE_WRITEONCE, "pintool", "s", "100", "specify the maximum spacing between two dependant instructions in the program");

//KNOB<string> HistoryOutputFile(KNOB_MODE_WRITEONCE, "pintool", "qqq", "history.txt", "specify the history file");

static UINT64 ins_count = 0;
void docount() {
	ins_count++;
}


static std::map<REG, UINT64> last_reg_write;
void updateRegAccessIns(REG reg) {
	last_reg_write[reg] = ins_count;
}


// Uncomment this and the code in updateSpacingInfo and Fini to write out the dependency history
// for each register used
//static std::map<REG, std::vector<UINT64>> reg_dependency_history;


// This function is called before every instruction is executed. Have to change
// the code to send in the register names from the Instrumentation function
VOID updateSpacingInfo(REG reg) {
	UINT64 spacing = ins_count - last_reg_write[reg];
	if (spacing >= 100) return; // sub 100 for maxSpacing because of int/uint comparison
	dependancySpacing[spacing]++;
	//reg_dependency_history[reg].push_back(spacing);
}


// Pin calls this function every time a new instruction is encountered
VOID Instruction(INS ins, VOID *v)
{
    // Insert a call to updateSpacingInfo before every instruction.
    // You may need to add arguments to the call.
	std::set<REG> written_registers;
	INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)docount, IARG_END);
	for (uint j = 0; j < INS_MaxNumWRegs(ins); j++) {
		REG write_reg;
		if (USE_FULL_REGISTERS_ONLY)
			write_reg = REG_FullRegName(INS_RegW(ins, j));
		else
			write_reg = INS_RegW(ins, j);

		// Don't instrument the same register more than once per instruction
		if (written_registers.find(write_reg) == written_registers.end())
			written_registers.insert(write_reg);
		else
			continue;

		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)updateRegAccessIns,
					   IARG_UINT32, write_reg, IARG_END);
	}

	std::set<REG> read_registers;
	for (uint i = 0; i < INS_MaxNumRRegs(ins); i++) {
		REG read_reg = REG_FullRegName(INS_RegR(ins, i));
		if (USE_FULL_REGISTERS_ONLY)
			read_reg = REG_FullRegName(INS_RegR(ins, i));
		else
			read_reg = INS_RegR(ins, i);

		// Don't record the same register more than once per instruction
		if (read_registers.find(read_reg) == read_registers.end())
			read_registers.insert(read_reg);
		else
			continue;

		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)updateSpacingInfo, 
					   IARG_UINT32, read_reg, IARG_END);
	}
}

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
	/*
	OutFile.open(HistoryOutputFile.Value().c_str());
	OutFile.setf(ios::showbase);
	for (std::map<REG, std::vector<UINT64>>::iterator it = reg_dependency_history.begin(); it != reg_dependency_history.end(); ++it)
	{
		OutFile << it->first << ": ";
		for (std::vector<UINT64>::iterator vt = it->second.begin(); vt != it->second.end(); ++vt)
		{
			OutFile << *vt << ", ";
		}
		OutFile << std::endl;
	}
	OutFile.close();
	*/

    // Write to a file since cout and cerr maybe closed by the application
    OutFile.open(KnobOutputFile.Value().c_str());
    OutFile.setf(ios::showbase);
	for(INT32 i = 0; i < maxSize; i++)
		OutFile << dependancySpacing[i]<<",";
    OutFile.close();
}

// argc, argv are the entire command line, including pin -t <toolname> -- ...
int main(int argc, char * argv[])
{
    // Initialize pin
    PIN_Init(argc, argv);

    //printf("Warning: Pin Tool not implemented\n");
    maxSize = atoi(KnobMaxSpacing.Value().c_str());

    // Initializing depdendancy Spacing
    dependancySpacing = new UINT64[maxSize];

    // Register Instruction to be called to instrument instructions
    INS_AddInstrumentFunction(Instruction, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);
    
    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}

