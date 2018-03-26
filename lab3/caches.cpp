#include <iostream>
#include <fstream> 
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include "pin.H"

UINT32 logPageSize;
UINT32 logPhysicalMemSize;

//Function to obtain physical page number given a virtual page number
UINT64 getPhysicalPageNumber(UINT64 virtualPageNumber)
{
    INT32 key = (INT32) virtualPageNumber;
    key = ~key + (key << 15); // key = (key << 15) - key - 1;
    key = key ^ (key >> 12);
    key = key + (key << 2);
    key = key ^ (key >> 4);
    key = key * 2057; // key = (key + (key << 3)) + (key << 11);
    key = key ^ (key >> 16);
    return (UINT32) (key&(((UINT32)(~0))>>(32-logPhysicalMemSize)));
}

class CacheModel
{
    protected:
        UINT32   logNumRows;
        UINT32   logBlockSize;
        UINT32   associativity;
        UINT64   readReqs;
        UINT64   writeReqs;
        UINT64   readHits;
        UINT64   writeHits;
        UINT32** tag;
        bool**   validBit;


		// Keeps track of set accesses for each row so we can
		// find the least recentely used set for eviction/replacement.
		UINT32** lru_history;

		// # of bits to right shift address to put tag in lower bits
		UINT32 tag_shift_bits;
		
		// Bitmasks for accessing the index and tag of an address.
		// Need to be calculated in each model's constructor.
		UINT32 tag_mask;
		UINT32 index_mask;

    public:
        //Constructor for a cache
        CacheModel(UINT32 logNumRowsParam, UINT32 logBlockSizeParam, UINT32 associativityParam)
        {
            logNumRows = logNumRowsParam;
            logBlockSize = logBlockSizeParam;
            associativity = associativityParam;
            readReqs = 0;
            writeReqs = 0;
            readHits = 0;
            writeHits = 0;
			
            tag = new UINT32*[1u<<logNumRows];
            validBit = new bool*[1u<<logNumRows];
			lru_history = new UINT32*[1u<<logNumRows];
            for(UINT32 i = 0; i < 1u<<logNumRows; i++)
            {
                tag[i] = new UINT32[associativity];
                validBit[i] = new bool[associativity];
				lru_history[i] = new UINT32[associativity];
                for(UINT32 j = 0; j < associativity; j++)
				{
                    validBit[i][j] = false;
					lru_history[i][j] = j;
				}
            }       
        }

        //Call this function to update the cache state whenever data is read
        virtual void readReq(UINT32 virtualAddr) {}

        //Call this function to update the cache state whenever data is written
        virtual void writeReq(UINT32 virtualAddr) {}

        //Do not modify this function
        virtual void dumpResults(ofstream *outfile)
        {
        	*outfile << readReqs <<","<< writeReqs <<","<< readHits <<","<< writeHits <<"\n";
        }

	protected:

		bool search_cache(UINT32 row, UINT32 address_tag) 
		{
			for (UINT32 i = 0; i < associativity; i++)
			{
				if (validBit[row][i] && tag[row][i] == address_tag)
				{
					update_lru_history(row, i);
					return true;
				}
			}

			UINT32 replace_index = get_lru_replacement_index(row);
			validBit[row][replace_index] = true;
			tag[row][replace_index] = address_tag;
			update_lru_history(row, replace_index);
			return false;
		}

		void update_lru_history(UINT32 row, UINT32 accessed_index)
		{
			UINT32 new_head = lru_history[row][accessed_index];
			for (UINT32 i = accessed_index; i > 0; i--) 
			{
				lru_history[row][i] = lru_history[row][i-1];
			}
			lru_history[row][0] = new_head;
		}

		UINT32 get_lru_replacement_index(UINT32 row) 
		{
			 return lru_history[row][associativity-1];
		} 	

};

CacheModel* cachePP;
CacheModel* cacheVP;
CacheModel* cacheVV;

class LruPhysIndexPhysTagCacheModel: public CacheModel
{
    public:
        LruPhysIndexPhysTagCacheModel(UINT32 logNumRowsParam, UINT32 logBlockSizeParam, UINT32 associativityParam)
            : CacheModel(logNumRowsParam, logBlockSizeParam, associativityParam)
        {
			// Create bitmasks for accessing the index and tag of an address
			index_mask = (1u << logNumRows) - 1;
			tag_shift_bits = logNumRows + logBlockSize;
			tag_mask = (1u << (32 - tag_shift_bits)) - 1;
        }

        void readReq(UINT32 virtualAddr)
        {
			UINT32 physicalAddr = getPhysicalPageNumber(virtualAddr);
			
			// We don't actually care about the block size since we assume
			// it's just zeroed out. Shift right to get rid of it and make
			// finding the index easier.
			UINT32 row = (physicalAddr >> logBlockSize) & index_mask;
			UINT32 address_tag = (physicalAddr >> tag_shift_bits) & tag_mask;  

			bool success = CacheModel::search_cache(row, address_tag);
			if (success)
				readHits++;
			readReqs++;
        }

        void writeReq(UINT32 virtualAddr)
        {
			UINT32 physicalAddr = getPhysicalPageNumber(virtualAddr);
			
			UINT32 row = (physicalAddr >> logBlockSize) & index_mask;
			UINT32 address_tag = (physicalAddr >> tag_shift_bits) & tag_mask;

			bool success = CacheModel::search_cache(row, address_tag);
			if (success)
				writeHits++;
			writeReqs++;
        }

		/*void dumpResults(ofstream *outfile) {
    		ofstream logfile;
		    logfile.open("reads.log");
			logfile.setf(ios::showbase);
			logfile << log.rdbuf();
			logfile.close();
			CacheModel::dumpResults(outfile);
		}
	*/
};

class LruVirIndexPhysTagCacheModel: public CacheModel
{
    public:
        LruVirIndexPhysTagCacheModel(UINT32 logNumRowsParam, UINT32 logBlockSizeParam, UINT32 associativityParam)
            : CacheModel(logNumRowsParam, logBlockSizeParam, associativityParam)
        {
			index_mask = (1u << logNumRows) -1;
			tag_shift_bits = logNumRows + logBlockSize;
			tag_mask = (1u << (32 - tag_shift_bits)) - 1;	
        }

        void readReq(UINT32 virtualAddr)
        {
			UINT32 physicalAddr = getPhysicalPageNumber(virtualAddr);
			UINT32 row = (virtualAddr >> logBlockSize) & index_mask;
			UINT32 address_tag = (physicalAddr >> tag_shift_bits) & tag_mask;

			bool success = CacheModel::search_cache(row, address_tag);
			if (success)
				readHits++;
			readReqs++;
        }

        void writeReq(UINT32 virtualAddr)
        {
			UINT32 physicalAddr = getPhysicalPageNumber(virtualAddr);
			UINT32 row = (virtualAddr >> logBlockSize) & index_mask;
			UINT32 address_tag = (physicalAddr >> tag_shift_bits) & tag_mask;

			bool success = CacheModel::search_cache(row, address_tag);
			if (success)
				writeHits++;
			writeReqs++;
		}
};

class LruVirIndexVirTagCacheModel: public CacheModel
{
    public:
        LruVirIndexVirTagCacheModel(UINT32 logNumRowsParam, UINT32 logBlockSizeParam, UINT32 associativityParam)
            : CacheModel(logNumRowsParam, logBlockSizeParam, associativityParam)
        {
        }

        void readReq(UINT32 virtualAddr)
        {
			CacheModel::readReq(virtualAddr);
        }

        void writeReq(UINT32 virtualAddr)
        {
			CacheModel::writeReq(virtualAddr);
        }
};

//Cache analysis routine
void cacheLoad(UINT32 virtualAddr)
{
    //Here the virtual address is aligned to a word boundary
    virtualAddr = (virtualAddr >> 2) << 2;
    cachePP->readReq(virtualAddr);
    cacheVP->readReq(virtualAddr);
    cacheVV->readReq(virtualAddr);
}

//Cache analysis routine
void cacheStore(UINT32 virtualAddr)
{
    //Here the virtual address is aligned to a word boundary
    virtualAddr = (virtualAddr >> 2) << 2;
    cachePP->writeReq(virtualAddr);
    cacheVP->writeReq(virtualAddr);
    cacheVV->writeReq(virtualAddr);
}

// This knob will set the outfile name
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
			    "o", "results.out", "specify optional output file name");

// This knob will set the param logPhysicalMemSize
KNOB<UINT32> KnobLogPhysicalMemSize(KNOB_MODE_WRITEONCE, "pintool",
                "m", "16", "specify the log of physical memory size in bytes");

// This knob will set the param logPageSize
KNOB<UINT32> KnobLogPageSize(KNOB_MODE_WRITEONCE, "pintool",
                "p", "12", "specify the log of page size in bytes");

// This knob will set the cache param logNumRows
KNOB<UINT32> KnobLogNumRows(KNOB_MODE_WRITEONCE, "pintool",
                "r", "10", "specify the log of number of rows in the cache");

// This knob will set the cache param logBlockSize
KNOB<UINT32> KnobLogBlockSize(KNOB_MODE_WRITEONCE, "pintool",
                "b", "5", "specify the log of block size of the cache in bytes");

// This knob will set the cache param associativity
KNOB<UINT32> KnobAssociativity(KNOB_MODE_WRITEONCE, "pintool",
                "a", "2", "specify the associativity of the cache");

// Pin calls this function every time a new instruction is encountered
VOID Instruction(INS ins, VOID *v)
{
    if(INS_IsMemoryRead(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)cacheLoad, IARG_MEMORYREAD_EA, IARG_END);
    if(INS_IsMemoryWrite(ins))
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)cacheStore, IARG_MEMORYWRITE_EA, IARG_END);
}

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
    ofstream outfile;
    outfile.open(KnobOutputFile.Value().c_str());
    outfile.setf(ios::showbase);
    outfile << "physical index physical tag: ";
    cachePP->dumpResults(&outfile);
     outfile << "virtual index physical tag: ";
    cacheVP->dumpResults(&outfile);
     outfile << "virtual index virtual tag: ";
    cacheVV->dumpResults(&outfile);
    outfile.close();
}

// argc, argv are the entire command line, including pin -t <toolname> -- ...
int main(int argc, char * argv[])
{
	// Initialize random seed
	srand(time(NULL));

    // Initialize pin
    PIN_Init(argc, argv);
	
    logPageSize = KnobLogPageSize.Value();
    logPhysicalMemSize = KnobLogPhysicalMemSize.Value();

    cachePP = new LruPhysIndexPhysTagCacheModel(KnobLogNumRows.Value(), KnobLogBlockSize.Value(), KnobAssociativity.Value()); 
    cacheVP = new LruVirIndexPhysTagCacheModel(KnobLogNumRows.Value(), KnobLogBlockSize.Value(), KnobAssociativity.Value());
    cacheVV = new LruVirIndexVirTagCacheModel(KnobLogNumRows.Value(), KnobLogBlockSize.Value(), KnobAssociativity.Value());

    // Register Instruction to be called to instrument instructions
    INS_AddInstrumentFunction(Instruction, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}
