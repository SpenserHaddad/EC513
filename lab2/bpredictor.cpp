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
  myBranchPredictor() { }

  BOOL makePrediction(ADDRINT address)
	{
		//this->debugStream << "Making prediction using " << std::bitset<8>(this->global_register_history) << std::endl;
		PREDICTOR predictor = this->pattern_history_table[this->global_register_history]; 
		//this->debugStream << "Prediction is: " << predictor_to_string(predictor) << std::endl;
		switch (predictor) {
			case STRONGLY_TAKEN:
				return TRUE;
			case WEAKLY_TAKEN:
				return TRUE;
			case WEAKLY_NOT_TAKEN:
				return FALSE;
			case STRONGLY_NOT_TAKEN:
				return FALSE;
			default:
				return TRUE;
		}
	}

  void makeUpdate(BOOL takenActually, BOOL takenPredicted, ADDRINT address) {
	//this->debugStream << "Result was " << takenActually << ", we said " << takenPredicted << std::endl;

		PREDICTOR old_predictor = this->pattern_history_table[this->global_register_history];
		PREDICTOR new_predictor = WEAKLY_TAKEN;
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
		//this->debugStream << "Updated prediction for " << std::bitset<8>(this->global_register_history) << " to " << predictor_to_string(new_predictor) << std::endl;
		this->pattern_history_table[this->global_register_history] = new_predictor;
		
		this->global_register_history = (this->global_register_history << 1) | takenActually;
/*		if (takenActually) {
			this->global_register_history |= 1;
		}*/
		//this->debugStream << "New GHR is: " << std::bitset<8>(this->global_register_history) << std::endl;

	}
 
  void Finish() {
		/*ofstream debugFile;
  		debugFile.open("debug.log");
		debugFile.setf(ios::showbase);

		debugFile << "Take 2" << std::endl << this->debugStream.str();
		debugFile.close();
		*/
	};


  private:
	UINT8 global_register_history = 0;
	PREDICTOR pattern_history_table[256] = { WEAKLY_TAKEN } ;
	std::stringstream debugStream;


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

