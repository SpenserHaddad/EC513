#include <iostream>
#include <fstream> 
#include <stdio.h>
#include <assert.h>
#include <math.h>
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
		UINT32** lruHistory;

		// The following values are model-dependant 

		// number of bits to right shift address to move index and tag bits to
		// the lower bits.
		UINT32 indexShiftBits;
		UINT32 tagShiftBits;
		
		// Bitmasks for accessing the index and tag of an address, after shifting 
		// the relevant section to the least significant bits.
		UINT32 tagMask;
		UINT32 indexMask;

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
			lruHistory = new UINT32*[1u<<logNumRows];
            for(UINT32 i = 0; i < 1u<<logNumRows; i++)
            {
                tag[i] = new UINT32[associativity];
                validBit[i] = new bool[associativity];
				lruHistory[i] = new UINT32[associativity];
                for(UINT32 j = 0; j < associativity; j++)
				{
                    validBit[i][j] = false;
					lruHistory[i][j] = j;
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
		
		// Traverses the cache at the given row for the tag.
		// Returns true if it finds the tag (aka cache hit).
		// Updates the cache structure after every search
		bool searchCache(UINT32 row, UINT32 addressTag) 
		{
			for (UINT32 i = 0; i < associativity; i++)
			{
				if (validBit[row][i] && tag[row][i] == addressTag)
				{
					// Found the address in the cache, update access history
					// and finish.
					updateLruHistory(row, i);
					return true;
				}
			}

			// Cache miss, "load" the value into the cache and 
			// update the lru history.
			UINT32 replaceIndex = getLruReplacementIndex(row);
			validBit[row][replaceIndex] = true;
			tag[row][replaceIndex] = addressTag;
			updateLruHistory(row, replaceIndex);
			return false;
		}

		// Moves the accessed history to the bottom of the lru stack.
		void updateLruHistory(UINT32 row, UINT32 accessedIndex)
		{
			UINT32 newHead = lruHistory[row][accessedIndex];
			// Move the rest of the history down first first.
			for (UINT32 i = accessedIndex; i > 0; i--) 
			{
				lruHistory[row][i] = lruHistory[row][i-1];
			}
			lruHistory[row][0] = newHead;
		}

		// Get the index of the least recently used element in the
		// cache row.
		UINT32 getLruReplacementIndex(UINT32 row) 
		{
			 return lruHistory[row][associativity-1];
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
			// Create bitmasks and shift constants for accessing 
			// the index and tag of an address.
			indexShiftBits = logBlockSize + logPageSize;
			indexMask = (1u << logNumRows) - 1;
			tagShiftBits = logNumRows + logBlockSize;
			tagMask = (1u << (32 - tagShiftBits)) - 1;
        }

        void readReq(UINT32 virtualAddr)
        {
			// Find the row and tag, then pass to the base class to search the cache.
			UINT32 physicalAddr = getPhysicalPageNumber(virtualAddr);
			// The virtual addresses index bits are the same as the physical address.
			UINT32 row = (virtualAddr >> indexShiftBits) & indexMask;
			UINT32 addressTag = (physicalAddr >> tagShiftBits) & tagMask;  

			bool success = CacheModel::searchCache(row, addressTag);
			if (success)
				readHits++;
			readReqs++;
        }

        void writeReq(UINT32 virtualAddr)
        {
			UINT32 physicalAddr = getPhysicalPageNumber(virtualAddr);
			
			UINT32 row = (physicalAddr >> indexShiftBits) & indexMask;
			UINT32 addressTag = (physicalAddr >> tagShiftBits) & tagMask;

			bool success = CacheModel::searchCache(row, addressTag);
			if (success)
				writeHits++;
			writeReqs++;
        }
};

class LruVirIndexPhysTagCacheModel: public CacheModel
{
    public:
        LruVirIndexPhysTagCacheModel(UINT32 logNumRowsParam, UINT32 logBlockSizeParam, UINT32 associativityParam)
            : CacheModel(logNumRowsParam, logBlockSizeParam, associativityParam)
        {
			indexShiftBits = logBlockSize + logPageSize;
			indexMask = (1u << logNumRows) -1;
			tagShiftBits = logNumRows + logBlockSize;
			tagMask = (1u << (32 - tagShiftBits)) - 1;	
        }

        void readReq(UINT32 virtualAddr)
        {
			UINT32 physicalAddr = getPhysicalPageNumber(virtualAddr);
			UINT32 row = (virtualAddr >> indexShiftBits) & indexMask;
			UINT32 addressTag = (physicalAddr >> tagShiftBits) & tagMask;

			bool success = CacheModel::searchCache(row, addressTag);
			if (success)
				readHits++;
			readReqs++;
        }

        void writeReq(UINT32 virtualAddr)
        {
			UINT32 physicalAddr = getPhysicalPageNumber(virtualAddr);
			UINT32 row = (virtualAddr >> indexShiftBits) & indexMask;
			UINT32 addressTag = (physicalAddr >> tagShiftBits) & tagMask;

			bool success = CacheModel::searchCache(row, addressTag);
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
			indexShiftBits = logBlockSize + logPageSize;
			indexMask = (1u << logNumRows) - 1;
			tagShiftBits = logNumRows + logBlockSize;
			tagMask = (1u << (32 - tagShiftBits)) - 1;
        }

        void readReq(UINT32 virtualAddr)
        {
			UINT32 row = (virtualAddr >> indexShiftBits) & indexMask;
			UINT32 addressTag = (virtualAddr >> tagShiftBits) & tagMask;

			bool success = CacheModel::searchCache(row, addressTag);
			if (success)
				readHits++;
			readReqs++;
        }

        void writeReq(UINT32 virtualAddr)
        {
			UINT32 row = (virtualAddr >> indexShiftBits) & indexMask;
			UINT32 addressTag = (virtualAddr >> tagShiftBits) & tagMask;

			bool success = CacheModel::searchCache(row, addressTag);
			if (success)
				writeHits++;
			writeReqs++;
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
