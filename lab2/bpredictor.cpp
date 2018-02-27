#include <iostream>
#include <stdio.h>
#include <assert.h>
#include <fstream> 
#include <set>
#include <sstream>
#include <string>
#include <bitset>
#include "pin.H"

static UINT64 takenCorrect = 0;
static UINT64 takenIncorrect = 0;
static UINT64 notTakenCorrect = 0;
static UINT64 notTakenIncorrect = 0;

enum PREDICTOR {
	STRONGLY_NOT_TAKEN = 0,
	WEAKLY_NOT_TAKEN,
	WEAKLY_TAKEN,
	STRONGLY_TAKEN
};


BOOL get_prediction(const PREDICTOR &predictor) {
	switch (predictor) {
		case STRONGLY_TAKEN:
		case WEAKLY_TAKEN:
			return TRUE;
		case WEAKLY_NOT_TAKEN:
		case STRONGLY_NOT_TAKEN:
			return FALSE;
		default:
			return TRUE;
		}
}


PREDICTOR get_new_pred_state(const PREDICTOR &old_predictor, const BOOL &takenActually) {
	PREDICTOR new_predictor = old_predictor;
	
	switch (old_predictor) {
		case STRONGLY_TAKEN:
			if (!takenActually)
				new_predictor = WEAKLY_TAKEN;
			break;
		case WEAKLY_TAKEN:
			if (takenActually)
				new_predictor = STRONGLY_TAKEN;
			else
				new_predictor = WEAKLY_NOT_TAKEN;
			break;
		case WEAKLY_NOT_TAKEN:
			if (takenActually)
				new_predictor = WEAKLY_TAKEN;
			else
				new_predictor = STRONGLY_NOT_TAKEN;
			break;
		case STRONGLY_NOT_TAKEN:
			if (takenActually)
				new_predictor = WEAKLY_NOT_TAKEN;
			break;
		}
	return new_predictor;
}

std::string predictor_to_string(PREDICTOR p) {
	switch(p) {
		case STRONGLY_NOT_TAKEN:
			return "STRONGLY NOT TAKEN";
		case WEAKLY_NOT_TAKEN:
			return "WEAKLY NOT TAKEN";
		case WEAKLY_TAKEN:
			return "WEAKLY TAKEN";
		case STRONGLY_TAKEN:
			return "STRONGLY TAKEN";
		default:
			return "";
	}
}

class BranchPredictor {

  public:
  BranchPredictor() { }

  virtual BOOL makePrediction(ADDRINT address) { return FALSE;};

  virtual void makeUpdate(BOOL takenActually, BOOL takenPredicted, ADDRINT address) {};
  
  virtual void Finish() {};	
};

class myBranchPredictor: public BranchPredictor {
  public:
  myBranchPredictor() {
	debugStream << "Init, table_mask = " << (UINT16)this->table_mask << std::endl;	
	debugFile.open("debug.log");
	}

  BOOL makePrediction(ADDRINT address)
	{
		debugStream << "Making prediction for " << address << std::endl;
		
		UINT8 table_index = (address & this->table_mask) & 0xFF;
		if (table_index >= 256) {
			debugStream << "***WARNING: TABLE_INDEX***" << std::endl;
			flush_debug_file();
		}
		debugStream << "\ttable_index = " << (UINT16)table_index;
		
		UINT8 grh_entry = this->address_histories[table_index];
		if (grh_entry >= 256) {
			debugStream << "***WARNING: GRH_ENTRY***" << std::endl;
			flush_debug_file();
		}
		debugStream <<", grh_entry = " << std::bitset<8>(grh_entry);
		
		PREDICTOR predictor = (PREDICTOR)this->pattern_history_table[grh_entry];
		debugStream <<", predictor = " << predictor_to_string(predictor);
		debugStream << std::endl;
		//flush_debug_file();
		return get_prediction(predictor);
	}

  void makeUpdate(BOOL takenActually, BOOL takenPredicted, ADDRINT address) {
		debugStream << "Updating for " << address << ", takenActually = " << takenActually << std::endl;
	
		UINT8 table_index = address & this->table_mask;
		if (table_index >= 256) {
			debugStream << "***WARNING: TABLE_INDEX***" << std::endl;
			flush_debug_file();
		}
		UINT8 grh_entry = this->address_histories[table_index];
		if (grh_entry >= 256) {
			debugStream << "***WARNING: GRH_ENTRY***" << std::endl;
			flush_debug_file();
		}
		PREDICTOR old_predictor = (PREDICTOR)this->pattern_history_table[grh_entry];
		
		PREDICTOR new_predictor = get_new_pred_state(old_predictor, takenActually);
		debugStream << "\tUpdating predictor for table_index = " << (UINT16)table_index << ", grh_entry = " << std::bitset<8>(grh_entry) << " to " << predictor_to_string(new_predictor) << std::endl;
		this->pattern_history_table[grh_entry] = new_predictor;
		this->address_histories[table_index] = (grh_entry << 1) | takenActually;
		
		debugStream << "\tgrh_entry updated to " << std::bitset<8>(this->address_histories[table_index]) << std::endl;
		flush_debug_file();
	}
 
  void Finish() {
/*		ofstream debugFile;
  		debugFile.open("debug.log");
		debugFile.setf(ios::showbase);

		debugFile << this->debugStream.str();
*/		debugFile.close();
	};


  private:
	UINT8 table_mask = 0xFF;
	UINT8 address_histories[256];
	UINT8 pattern_history_table[256] = { WEAKLY_TAKEN } ;
	std::stringstream debugStream;
	ofstream debugFile;
	
	void flush_debug_file() {
		debugFile << this->debugStream.str();
		this->debugStream.str(std::string());
		debugStream.clear();
		debugFile.flush();
	}
};

BranchPredictor* BP;


// This knob sets the output file name
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "result.out", "specify the output file name");

KNOB<string> KnobDebugFile(KNOB_MODE_WRITEONCE, "pintool", "debug", "debug.log", "set the predictor's debug log");


// In examining handle branch, refer to quesiton 1 on the homework
void handleBranch(ADDRINT ip, BOOL direction)
{
  BOOL prediction = BP->makePrediction(ip);
  BP->makeUpdate(direction, prediction, ip);
  
  if(prediction) {
    if(direction) {
      takenCorrect++;
    }
    else {
      takenIncorrect++;
    }
  } else {
    if(direction) {
      notTakenIncorrect++;
    }
    else {
      notTakenCorrect++;
    }
  }
}


void instrumentBranch(INS ins, void * v)
{   
  if(INS_IsBranch(ins) && INS_HasFallThrough(ins)) {
    INS_InsertCall(
      ins, IPOINT_TAKEN_BRANCH, (AFUNPTR)handleBranch,
      IARG_INST_PTR,
      IARG_BOOL,
      TRUE,
      IARG_END); 

    INS_InsertCall(
      ins, IPOINT_AFTER, (AFUNPTR)handleBranch,
      IARG_INST_PTR,
      IARG_BOOL,
      FALSE,
      IARG_END);
  }
}
 

/* ===================================================================== */
VOID Fini(int, VOID * v)
{   
  BP->Finish();
  ofstream outfile;	
  outfile.open(KnobOutputFile.Value().c_str());
  outfile.setf(ios::showbase);
  outfile << "takenCorrect: "<< takenCorrect <<"  takenIncorrect: "<< takenIncorrect <<" notTakenCorrect: "<< notTakenCorrect <<" notTakenIncorrect: "<< notTakenIncorrect <<"\n";
  outfile.close();
}


// argc, argv are the entire command line, including pin -t <toolname> -- ...
int main(int argc, char * argv[])
{
    // Make a new branch predictor
    BP = new myBranchPredictor();

    // Initialize pin
    PIN_Init(argc, argv);

    // Register Instruction to be called to instrument instructions
    INS_AddInstrumentFunction(instrumentBranch, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);
    
    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}

