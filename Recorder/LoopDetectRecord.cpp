#include "pin.H"
#include <stdlib.h>
#include <stdio.h>

struct BBPathInfo {
    ADDRINT head;
};

struct BBInfo {
    ADDRINT head = 0; //Address of the BB/first instruction
    bool contains_call = false;
    bool contains_ret = false;
    ADDRINT retaddr = 0; // Only nonzero if contains call

    size_t n_ins = 0;
    ADDRINT instructions[250];
};


FILE * savefilefds[250]; // Max 250 threads.
uint32_t threadcount = 0;

void dump_bbinfo(FILE * fd, BBInfo *bbinfo) {
    fprintf(fd, "%d\n", 0); // 0 for BBinfo
    fprintf(
        fd, "%lx,%d,%d,%lx,",
        bbinfo->head,
        bbinfo->contains_call,
        bbinfo->contains_ret,
        bbinfo->retaddr
    ); // First 4 members, n_ins need not to be recorded

    // Instruction addrs
    for (size_t j=0; j < bbinfo->n_ins; j++) {
        fprintf(fd, "%lx,", bbinfo->instructions[j]);
    }
    fprintf(fd, "\n");
}

void dump_bbpath(FILE * fd, BBPathInfo *bpi) {
    fprintf(fd, "%d\n", 1); // 1 for BBPathinfo
    fprintf(fd, "%lx,\n", bpi->head); // First for members
}

void recordBB(ADDRINT bbhead, THREADID threadIndex) {
    BBPathInfo bb;
    bb.head = bbhead;
    dump_bbpath(savefilefds[threadIndex], &bb);
}

void LoopDetectRecord(TRACE trace, VOID *v) {
    THREADID threadIndex = PIN_ThreadId();
    
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {

        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR) recordBB, IARG_ADDRINT, BBL_Address(bbl), IARG_THREAD_ID, IARG_END);
        BBInfo bbinfo = BBInfo();
        bbinfo.head = BBL_Address(bbl);

        // Prepare instruction info in the basic block
        INS ins;
        for (ins = BBL_InsHead(bbl); ; ins = INS_Next(ins)) {
            if (bbinfo.n_ins == 250) {
                break;
            }
            bbinfo.instructions[bbinfo.n_ins++] = INS_Address(ins);
            /* Debug */
            // bbinfo->inst_disassem[INS_Address(ins)] = INS_Disassemble(ins);
            /* End Debug */
            if (ins == BBL_InsTail(bbl)) {
                if (INS_IsCall(ins)) {
                    bbinfo.contains_call = true;
                    bbinfo.retaddr = INS_NextAddress(ins);
                }
                if (INS_IsRet(ins)) {
                    bbinfo.contains_ret = true;
                }
                
                break;
            }  
        }
        dump_bbinfo(savefilefds[threadIndex], &bbinfo);
    }
}

VOID Fini(INT32 code, VOID *v) {
    
}

VOID ThreadFini(THREADID threadIndex, const CONTEXT *ctxt, INT32 code, VOID *v) {
    fflush(savefilefds[threadIndex]);
    fclose(savefilefds[threadIndex]);
    fprintf(stderr, "Trace Dump for thread %d finished; \n", threadIndex);
}

VOID ThreadStart(THREADID threadIndex, CONTEXT* ctxt, INT32 flags, VOID* v) {
    fprintf(stderr, "Thread %d started.\n", threadIndex);
    if (!(threadcount < 250)) {
        fprintf(stderr, "Max number of thread reached. Exiting...\n");
        PIN_ExitProcess(-1);
    }
    char filename[250];
    sprintf(filename, "record%d.txt", threadcount++);
    savefilefds[threadIndex] = fopen(filename, "w");
}

int main(int argc, char * argv[]){
    // Initialize pin
    //PIN_InitSymbols();
    PIN_InitSymbolsAlt(SYMBOL_INFO_MODE(UINT32(IFUNC_SYMBOLS) | UINT32(DEBUG_OR_EXPORT_SYMBOLS)));
    if (PIN_Init(argc, argv)) {
        fprintf(stderr, "Error on starting Pin Tool.\n");
        return 1;
    }


    // Register ThreadStart to be called when a thread starts.
    PIN_AddThreadStartFunction(ThreadStart, NULL);

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