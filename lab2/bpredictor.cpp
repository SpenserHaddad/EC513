#include <iostream>
#include <stdio.h>
#include <assert.h>
#include <fstream> 
#include <set>
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
		this->table_mask = (1 << this->table_bit_count) - 1;
	}

  BOOL makePrediction(ADDRINT address)
	{
		UINT64 table_index = address & this->table_mask;
		switch (this->history_table[table_index]) {
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

  void makeUpdate(BOOL takenActually, BOOL takenPredicted, ADDRINT address) {
		UINT64 table_index = address & this->table_mask;
		switch (this->history_table[table_index]) {
			case STRONGLY_TAKEN:
				if (!takenActually)
					this->history_table[table_index] = WEAKLY_TAKEN;
				break;
			case WEAKLY_TAKEN:
				if (takenActually)
					this->history_table[table_index] = STRONGLY_TAKEN;
				else
					this->history_table[table_index] = WEAKLY_NOT_TAKEN;
				break;
			case WEAKLY_NOT_TAKEN:
				if (takenActually)
					this->history_table[table_index] = WEAKLY_TAKEN;
				else
					this->history_table[table_index] = STRONGLY_NOT_TAKEN;
				break;
			case STRONGLY_NOT_TAKEN:
				if (takenActually)
					this->history_table[table_index] = WEAKLY_NOT_TAKEN;
				break;
		}
	}
 
  void Finish() {};


  private:
	const UINT64 table_bit_count = 16;
	UINT64 table_mask = 0;
	PREDICTOR history_table[65536];


};
BranchPredictor* BP;


// This knob sets the output file name
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "result.out", "specify the output file name");


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

