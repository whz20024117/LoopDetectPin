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


FILE * fd;

BBInfo **basicBlockInfos = nullptr;
size_t bbis_size = 0;
size_t bbis_capacity = 0;
BBPathInfo **BBdumps = nullptr;
size_t bbds_size = 0;
size_t bbds_capacity = 0;

// Utility functions for saving data.
void append_bbis(BBInfo *bbi) {
    if (bbis_capacity == bbis_size) {
        size_t new_capacity = bbis_capacity + bbis_capacity / 2;
        BBInfo **new_basicBlockInfos = new BBInfo*[new_capacity];

        for (size_t i=0; i < bbis_size; i++) {
            new_basicBlockInfos[i] = basicBlockInfos[i];
        }
        delete[] basicBlockInfos;
        basicBlockInfos = new_basicBlockInfos;
        bbis_capacity = new_capacity;
    }

    basicBlockInfos[bbis_size++] = bbi;
}

void append_bbds(BBPathInfo *bbd) {
    if (bbds_capacity == bbds_size) {
        size_t new_capacity = bbds_capacity + bbds_capacity / 2;
        BBPathInfo **new_BBdumps = new BBPathInfo*[new_capacity];

        for (size_t i=0; i < bbds_size; i++) {
            new_BBdumps[i] = BBdumps[i];
        }
        delete[] BBdumps;
        BBdumps = new_BBdumps;
        bbds_capacity = new_capacity;
    }

    BBdumps[bbds_size++] = bbd;
}

void recordBB(ADDRINT bbhead) {
    BBPathInfo *bb = new BBPathInfo;
    bb->head = bbhead;
    append_bbds(bb);
}

void LoopDetectRecord(TRACE trace, VOID *v) {
    
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {

        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR) recordBB, IARG_ADDRINT, BBL_Address(bbl), IARG_END);
        BBInfo *bbinfo = new BBInfo;
        bbinfo->head = BBL_Address(bbl);

        // Prepare instruction info in the basic block
        INS ins;
        for (ins = BBL_InsHead(bbl); ; ins = INS_Next(ins)) {
            if (bbinfo->n_ins == 250) {
                break;
            }
            bbinfo->instructions[bbinfo->n_ins++] = INS_Address(ins);
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
        append_bbis(bbinfo);
    }
}

void dump_bbinfo() {
    fprintf(stdout, "Total bbinfo size: %lu\n", bbis_size);
    for (size_t i=0; i < bbis_size; i++) {
        fprintf(fd, "%d\n", 0); // 0 for BBinfo
        fprintf(
            fd, "%lx,%d,%d,%lx,",
            basicBlockInfos[i]->head,
            basicBlockInfos[i]->contains_call,
            basicBlockInfos[i]->contains_ret,
            basicBlockInfos[i]->retaddr
        ); // First for members

        // Instruction addrs
        for (size_t j=0; j < basicBlockInfos[i]->n_ins; j++) {
            fprintf(fd, "%lx,", basicBlockInfos[i]->instructions[j]);
        }
        fprintf(fd, "\n");

        delete basicBlockInfos[i];
    }
}

void dump_bbpath() {
    fprintf(stdout, "Total bbpath size: %lu\n", bbds_size);
    for (size_t i=0; i < bbds_size; i++) {
        fprintf(fd, "%d\n", 1); // 1 for BBPathinfo
        fprintf(fd, "%lx,\n", BBdumps[i]->head); // First for members

        delete BBdumps[i];
    }
}

VOID Fini(INT32 code, VOID *v) {
    
}

VOID ThreadFini(THREADID threadIndex, const CONTEXT *ctxt, INT32 code, VOID *v) {
    dump_bbinfo();
    dump_bbpath();
    fprintf(stdout, "Trace Dump finished; \n");
}

int main(int argc, char * argv[]){
    // Initialize pin
    //PIN_InitSymbols();
    PIN_InitSymbolsAlt(SYMBOL_INFO_MODE(UINT32(IFUNC_SYMBOLS) | UINT32(DEBUG_OR_EXPORT_SYMBOLS)));
    if (PIN_Init(argc, argv)) {
        fprintf(stderr, "Error on starting Pin Tool.\n");
        return 1;
    }

    // Buffer init
    basicBlockInfos = new BBInfo*[500];
    bbis_capacity = 500;
    BBdumps = new BBPathInfo*[5000];
    bbds_capacity = 5000;


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