#include <iostream>
#include <stdio.h>
#include <assert.h>
#include <fstream> 
#include <set>
#include <sstream>
#include <string>
#include <bitset>
#include "pin.H"

#define NUM_ADDRESS_TABLE_ENTRIES 512 
#define NUM_PATTERN_HIST_TABLE_ENTRIES 512 

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

class BranchPredictor {

  public:
  BranchPredictor() { }

  virtual BOOL makePrediction(ADDRINT address) { return FALSE;};

  virtual void makeUpdate(BOOL takenActually, BOOL takenPredicted, ADDRINT address) {};
  
  virtual void Finish() {};	
};


class gsharePredictor: public BranchPredictor {
	public:
	gsharePredictor() {}
	
	BOOL makePrediction(ADDRINT address) {
		UINT16 predictor_index = (address ^ history_register) & mask;
		return get_prediction((PREDICTOR)predictors[predictor_index]);
	}

	void makeUpdate(BOOL takenActually, BOOL takenPredicted, ADDRINT address) {
		UINT16 predictor_index = (address ^ history_register) & mask;
		PREDICTOR old_predictor = (PREDICTOR)predictors[predictor_index];
		PREDICTOR new_predictor = get_new_pred_state(old_predictor, takenActually);
		predictors[predictor_index] = (UINT8)new_predictor;

		history_register = (history_register << 1) & takenActually;
	}


	private:
	UINT16 history_register;
	UINT8 predictors[4096] = { WEAKLY_TAKEN};
	UINT16 mask = 0x7FF;
};


class twoLevelAdaptivePredictor: public BranchPredictor {
	public:
	twoLevelAdaptivePredictor() {
		address_index_mask = NUM_ADDRESS_TABLE_ENTRIES - 1;
		history_index_mask = NUM_PATTERN_HIST_TABLE_ENTRIES - 1;
	}

	BOOL makePrediction(ADDRINT address) {
		UINT16 address_index = address & address_index_mask;
		UINT8 grh_entry = address_branch_histories[address_index] & history_index_mask;
		PREDICTOR predictor = (PREDICTOR)address_pattern_histories[grh_entry];
		return get_prediction(predictor);
	}

  void makeUpdate(BOOL takenActually, BOOL takenPredicted, ADDRINT address) {
		UINT16 address_index = address & address_index_mask;
		UINT16 grh_entry = address_branch_histories[address_index];
		UINT16 grh_index = grh_entry & history_index_mask;
		PREDICTOR old_predictor = (PREDICTOR)address_pattern_histories[grh_index];
		
		PREDICTOR new_predictor = get_new_pred_state(old_predictor, takenActually);
		address_pattern_histories[grh_index] = new_predictor;
		address_branch_histories[address_index] = (grh_entry << 1) | takenActually;
	}
 
  void Finish() {};


	private:
	UINT16 address_index_mask;
	UINT16 history_index_mask;

	UINT8 address_branch_histories[NUM_ADDRESS_TABLE_ENTRIES];
	UINT8 address_pattern_histories[NUM_PATTERN_HIST_TABLE_ENTRIES] = { WEAKLY_TAKEN } ;
};


class myBranchPredictor: public BranchPredictor {
	public:
	myBranchPredictor() {
		bp1 = new gsharePredictor;
		bp2 = new twoLevelAdaptivePredictor;
	}

	BOOL makePrediction(ADDRINT address) {
		if (selector <=1)
			return bp1->makePrediction(address);
		else
			return bp2->makePrediction(address);
	}

	void makeUpdate(BOOL takenActually, BOOL takenPredicted, ADDRINT address) {
		bp1->makeUpdate(takenActually, takenPredicted, address);		
		bp2->makeUpdate(takenActually, takenPredicted, address);

		if (takenPredicted == takenActually) {
			if (selector == 1)
				selector = 0;
			else if (selector == 2)
				selector = 3;
		} else {
			if (selector <= 1)
				selector++;
			else
				selector--;
		}
	}

	private:
		BranchPredictor* bp1;
		BranchPredictor* bp2;
		UINT8 selector = 1; // 0-1, choose bp1. 2-3, choose bp2

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

