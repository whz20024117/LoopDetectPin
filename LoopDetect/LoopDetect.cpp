#include "LoopDetect.h"

#include <utility>
#include <iostream>
#include <map>

// Globals used by CallStack class
std::map<ADDRINT, BBInfo *> basicBlocks;
std::map<ADDRINT, LoopInfo *> loops;

bool enable_dbg_print = false;

static BBInfo *get_bbinfo(ADDRINT addr) {
    if (basicBlocks.find(addr) != basicBlocks.end()) {
        return basicBlocks[addr];
    } else {
        // Middle of a BB
        for (auto bit : basicBlocks) {
            ADDRINT head = bit.second->instructions[0];
            ADDRINT tail = bit.second->instructions.back();
            if (addr > head && addr < tail) {
                return bit.second;
            }
        }
    }

    return nullptr;
}

void StackFrame::popBB() {
    if (!path.empty()) {
        delete path.back();
        path.pop_back();
    } else {
        std::cerr << "No more BB to pop. \n";
        PIN_ExitProcess(1);
    }
}


void CallStack::popFrame() {
    assert(!callStack.empty());

    StackFrame *frame = callStack.back();
    std::vector<ADDRINT> inners;

    for (auto l : frame->topLoops) {
        inners.push_back(l);
    }

    // Pop the top Frame
    delete callStack.back();
    callStack.pop_back();
    if (callStack.empty())
        return;

    // Inherit inner loops
    frame = callStack.back(); // Get the new stack top
    if (frame->path.size() == 0) {
        if (inners.size() > 0) {
            std::cerr << "Warning: inner loops cannot find parent function. They will be discarded." << endl;
        }
        return;
    }
    BBInfo *bbinfo = basicBlocks[frame->path.back()->head];
    for (ADDRINT l : inners) {
        bbinfo->innerLoops.insert(l);
    }
}


void CallStack::popBB() {
    assert(!callStack.empty());
    StackFrame *frame = callStack.back();
    assert(!frame->path.empty());

    frame->popBB();
}

void CallStack::pushBB(BBPathInfo *bpi) {
    assert(!callStack.empty());
    StackFrame *frame = callStack.back();

    // Increment the iteration (executed times)
    assert(basicBlocks.find(bpi->head) != basicBlocks.end());
    basicBlocks[bpi->head]->iter += 1;

    frame->path.push_back(bpi);
}

void CallStack::newFrame() {
    StackFrame *frame = new StackFrame();
    if (!frame) {
        std::cerr << "Error pushing frame. \n";
        PIN_ExitProcess(1);
    }
    
    frame->retaddr = last_call_retaddr;
    callStack.push_back(frame);
}

void CallStack::adjustCallStack(ADDRINT bbhead) {
    if (returned) { // Last BB has return
        StackFrame *frame;

        /* First check if the return destination is in our stack... */
        bool ret_dest_in_stack = false;
        for (int i = callStack.size() - 1; i >=0; i--) {
            if (callStack[i]->retaddr == get_bbinfo(bbhead)->head) {
                ret_dest_in_stack = true;
                break;
            }
        }

        while (ret_dest_in_stack) {
            frame = getTopFrame();
            if (!frame || frame->path.empty()) {
                std::cerr << "Adjust Call Stack meet invalid return! bbhead = 0x" << std::hex << bbhead << std::endl;
                break;
            }
            if (get_bbinfo(bbhead)->head == get_bbinfo(frame->path.back()->head)->retaddr)
                break;
            popFrame();
        }
    } else if (called) {
        if (last_call_retaddr == 0) {
            std::cerr << "Invalid last_call_retaddr!!!!!!!." << std::endl;
        } else {
            newFrame();
        }
    }

    // Setup
    if (get_bbinfo(bbhead)->contains_ret) {
        returned = true;
    } else {
        returned = false;
    }
    if (get_bbinfo(bbhead)->contains_call) {
        called = true;
        last_call_retaddr = get_bbinfo(bbhead)->retaddr;
    } else {
        called = false;
        last_call_retaddr = 0;
    }
}

StackFrame *CallStack::getTopFrame() {
    if (callStack.empty()) {
        return nullptr;
    }
    return callStack.back();
}

BBPathInfo *CallStack::isInPath(ADDRINT bbhead) {
    StackFrame *frame = getTopFrame();
    
    // Search from back
    for (int i = frame->path.size() - 1; i >=0; i--) {
        // std::cerr << "DBG: " << i << ": "<< frame.path[i]->head << '\n';
        if (frame->path[i]->head == bbhead) {
            return frame->path[i];
        } else if (frame->path[i]->head < bbhead) {
            // Get the address of the last instruction
            ADDRINT tail = basicBlocks[frame->path[i]->head]->instructions.back(); 
            if (tail && tail > bbhead) {
                return frame->path[i];
            }
        }
    }
    return nullptr;
}

void CallStack::newBB(ADDRINT head) {
    BBPathInfo *bpi = new BBPathInfo();
    bpi->head = head;

    StackFrame *frame = getTopFrame();
    frame->path.push_back(bpi);
}

void CallStack::printCallStack(bool printBB=false) {
    std::cerr << "Printing current Call Stack: " << std::endl;
    for (int j = callStack.size() - 1; j >=0; j--) {
        ADDRINT retaddr = callStack[j]->retaddr;
        ADDRINT frameaddr = callStack[j]->path[0]->head;
        std::cerr << std::hex << "    Frame at 0x" << frameaddr << " with Ret address: 0x"  << retaddr << std::endl;
        if (printBB) {
            if (callStack[j]->path.size() == 0) {
                std::cerr << "        No BB in this frame." << std::endl;
            }
            for (int i = callStack[j]->path.size() - 1; i >=0; i--) {
                BBInfo *cur_bbinfo = get_bbinfo(callStack[j]->path[i]->head);
                std::cerr << std::hex << "        BB Head: 0x" << cur_bbinfo->head << " BB Tail: 0x" << cur_bbinfo->instructions.back() << std::endl;
            }
        }
    }
}

// Global variable stack, will be accessed by instrumentation functions
CallStack stack;

void processLoop(BBPathInfo *loopheadBB) {
    // std::cerr << "Process Loop" <<'\n';
    if (loops.find(loopheadBB->head) == loops.end()) {
        loops[loopheadBB->head] = new LoopInfo();
        loops[loopheadBB->head]->head = loopheadBB->head;
    }

    StackFrame *frame = stack.getTopFrame();

    BBInfo *bbinfo_loophead = basicBlocks[loopheadBB->head];
    BBInfo *bbinfo_loopbackbb = basicBlocks[frame->path.back()->head];

    bbinfo_loopbackbb->loopEdges.insert(bbinfo_loophead); // Add loopback edge for BB
    frame->topLoops.insert(loopheadBB->head); // Record top-level loops in the frame;


    LoopInfo *curloop = loops[loopheadBB->head];
    curloop->iter += 1; // Increment counter

    // Associate instructions and BBs to the loop
    for (int i = frame->path.size() - 1; ; i--) {
        assert(i>=0);

        ADDRINT startingAddress = frame->path[i]->head;
        BBInfo *bbinfo_current = basicBlocks[startingAddress];

        // Associate BB
        bbinfo_current->associatedTopLoop = curloop->head;

        // Nested loop cases: bbl is already a head for another loop L; This means L is a child of current loop.
        if (loops.find(bbinfo_current->head) != loops.end()) {
            LoopInfo *childLoop = loops[bbinfo_current->head];
            curloop->children.insert(childLoop);
        }

        for (ADDRINT insaddr : bbinfo_current->instructions) {
            // Associate instruction
            curloop->associatedInsts.insert(insaddr);
        }

        if (frame->path[i] == loopheadBB)
            break;
    }

    // Now pop the BBs until the loop head
    while (frame->path.back() != loopheadBB) {
        frame->popBB(); //TODO: add BBPathInfo pop function
    }
}



void processBB(ADDRINT bbhead) {
    stack.adjustCallStack(bbhead);

    if (stack.callStack.empty()) {
        // Before the dynamic linker (e.g. ld-linux.so) set up everything, we may encounter situation when
        // there is no stack frame in the stack but there are still BBs coming..
        return;
    }
    BBPathInfo *bpi = stack.isInPath(bbhead);

    if (bpi) {
        processLoop(bpi);
    } else {
        stack.newBB(bbhead);
    }
}


void LoopDetect(TRACE trace, VOID *v) {
    
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {

        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR) processBB, IARG_ADDRINT, BBL_Address(bbl), IARG_END);
        BBInfo *bbinfo = new BBInfo();
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

        basicBlocks[bbinfo->head] = bbinfo;
    }
}

void linkInnerLoop() {
    // Perform inner loop linking
    for (auto bbit : basicBlocks) {
        BBInfo *bbinfo = bbit.second;
        if (!bbinfo->innerLoops.empty() && bbinfo->associatedTopLoop) {
            for (ADDRINT l : bbinfo->innerLoops) {
               loops[bbinfo->associatedTopLoop]->children.insert(loops[l]); 
            }
        }
    }
}

void WrapUp() {
    linkInnerLoop();
    
    std::cerr << "################ loop Results ##################" << std::endl;
    
    std::map<ADDRINT, ADDRINT> address2loopid;
    for (auto loop : loops) {

        cout<<"Loop Head Basic Block: 0x"<< std::hex <<loop.second->head<<endl;
        
        for (auto I : loop.second->associatedInsts) {
            address2loopid[I] = loop.second->head;
        }
    }
}

VOID ThreadFini(THREADID threadIndex, const CONTEXT *ctxt, INT32 code, VOID *v) {
    WrapUp();
}


int main(int argc, char * argv[]){
    // Initialize pin
    //PIN_InitSymbols();
    PIN_InitSymbolsAlt(SYMBOL_INFO_MODE(UINT32(IFUNC_SYMBOLS) | UINT32(DEBUG_OR_EXPORT_SYMBOLS)));
    if (PIN_Init(argc, argv)) {
        std::cerr << "Error on starting Pin Tool." << std::endl;
        return 1;
    }

    // Register ThreadStart to be called when a thread starts.
    // PIN_AddThreadStartFunction(ThreadStart, NULL);

    // Register Fini to be called when thread exits.
    PIN_AddThreadFiniFunction(ThreadFini, NULL);

    // Register Fini to be called when the application exits.
    // PIN_AddFiniFunction(Fini, NULL);

    // Register Trace to be called to instrument instructions.
    TRACE_AddInstrumentFunction(LoopDetect, NULL);

    // Register ImageLoad to be called when loading images.
    // IMG_AddInstrumentFunction(ImageLoad, 0);

    // Start the program, never returns
    PIN_StartProgram();

    return 1;
}

