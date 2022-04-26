#include "pin.H"
#include <stdlib.h>
#include <stdio.h>
#include "containers.h"

struct BBPathInfo {
    ADDRINT head;
};

struct BBInfo {
    ADDRINT head = 0; //Address of the BB/first instruction
    bool contains_call = false;
    bool contains_ret = false;
    ADDRINT retaddr = 0; // Only nonzero if contains call

    MyVector<ADDRINT> instructions;
};


FILE * fd;

MyVector<BBInfo *> basicBlocks;
MyVector<BBPathInfo *> BBdumps;

void recordBB(ADDRINT bbhead) {
    BBPathInfo *bb = new BBPathInfo;
    bb->head = bbhead;
    BBdumps.push_back(bb);
}

void LoopDetectRecord(TRACE trace, VOID *v) {
    
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {

        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR) recordBB, IARG_ADDRINT, BBL_Address(bbl), IARG_END);
        BBInfo *bbinfo = new BBInfo;
        bbinfo->head = BBL_Address(bbl);

        // Prepare instruction info in the basic block
        INS ins;
        for (ins = BBL_InsHead(bbl); ; ins = INS_Next(ins)) {
            bbinfo->instructions.push_back(INS_Address(ins));
            /* Debug */
            // bbinfo->inst_disassem[INS_Address(ins)] = INS_Disassemble(ins);
            /* End Debug */
            if (ins == BBL_InsTail(bbl)) {
                if (INS_IsCall(ins)) {
                    bbinfo->contains_call = true;
                    bbinfo->retaddr = INS_NextAddress(ins);
                }
                if (INS_IsRet(ins)) {
                    bbinfo->contains_ret = true;
                }
                
                break;
            }  
        }
        basicBlocks.push_back(bbinfo);
    }
}

void dump_bbinfo() {
    for (size_t i=0; i < basicBlocks.size(); i++) {
        fprintf(fd, "%d\n", 0); // 0 for BBinfo
        fprintf(
            fd, "%lx,%d,%d,%lx,",
            basicBlocks[i]->head,
            basicBlocks[i]->contains_call,
            basicBlocks[i]->contains_ret,
            basicBlocks[i]->retaddr
        ); // First for members

        // Instruction addrs
        for (size_t j=0; j < basicBlocks[i]->instructions.size(); j++) {
            fprintf(fd, "%lx,", basicBlocks[i]->instructions[j]);
        }
        fprintf(fd, "\n");

        delete basicBlocks[i];
    }
}

void dump_bbpath() {
    for (size_t i=0; i < BBdumps.size(); i++) {
        fprintf(fd, "%d\n", 1); // 1 for BBPathinfo
        fprintf(fd, "%lx,\n", BBdumps[i]->head); // First for members

        delete BBdumps[i];
    }
}

VOID Fini(INT32 code, VOID *v) {
    dump_bbinfo();
    dump_bbpath();
}

VOID ThreadFini(THREADID threadIndex, const CONTEXT *ctxt, INT32 code, VOID *v) {
    
}

int main(int argc, char * argv[]){
    // Initialize pin
    //PIN_InitSymbols();
    PIN_InitSymbolsAlt(SYMBOL_INFO_MODE(UINT32(IFUNC_SYMBOLS) | UINT32(DEBUG_OR_EXPORT_SYMBOLS)));
    if (PIN_Init(argc, argv)) {
        std::cerr << "Error on starting Pin Tool." << std::endl;
        return 1;
    }

    // Open file
    fd = fopen("loop_detect_record.txt", "w");

    // Register ThreadStart to be called when a thread starts.
    // PIN_AddThreadStartFunction(ThreadStart, NULL);

    // Register Fini to be called when thread exits.
    PIN_AddThreadFiniFunction(ThreadFini, NULL);

    // Register Fini to be called when the application exits.
    PIN_AddFiniFunction(Fini, NULL);

    // Register Trace to be called to instrument instructions.
    TRACE_AddInstrumentFunction(LoopDetectRecord, NULL);

    // Start the program, never returns
    PIN_StartProgram();

    return 1;
}