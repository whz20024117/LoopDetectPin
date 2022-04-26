#ifndef LOOPDETECT_H
#define LOOPDETECT_H

#include "pin.H"
#include "containers.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <utility>
#include <string>
#include <cstring>

using std::string;
using std::ofstream;
using std::cout;
using std::endl;

struct LoopInfo {
    ADDRINT head;
    uint64_t iter = 1;
    uint64_t level;
    MySet<ADDRINT> associatedInsts;
    MySet<LoopInfo *> children;
};

struct BBPathInfo {
    ADDRINT head;
    ADDRINT sp; // Not used...
};

struct BBInfo {
    ADDRINT head = 0; //Address of the BB/first instruction
    uint64_t iter = 1;
    bool contains_call = false;
    bool contains_ret = false;
    ADDRINT retaddr = 0; // Only nonzero if contains call

    MySet<BBInfo *> loopEdges; // Backedge to loop head, if this BB is an end of the loop
    MyVector<ADDRINT> instructions;
    MyMap<ADDRINT, std::string> inst_disassem; // Debug
    ADDRINT associatedTopLoop = 0; // The most "outer"/"parent" loop associated to this BB. TODO: Possible Bugs here
    MySet<ADDRINT> innerLoops;
};

struct StackFrame {
    ADDRINT retaddr; // Address to continue execution after return (this frame popped)
    
    MyVector<BBPathInfo *> path;
    MySet<ADDRINT> topLoops;

    void popBB();
};


class CallStack {
    bool called = false; // Last processed BB contains call
    ADDRINT last_call_retaddr = 0;
    bool returned = false; // Last processed BB contains return

public:
    MyVector<StackFrame *> callStack;
    void popFrame();
    void popBB();

    void pushBB(BBPathInfo *bpi);
    void printCallStack(bool printBB);

    void newFrame();
    void newBB(ADDRINT head);

    StackFrame *getTopFrame();
    BBPathInfo *isInPath(ADDRINT bbhead);

    void adjustCallStack(ADDRINT bbhead);
};


void LoopProf(TRACE trace, VOID *v);
void WrapUp();

#endif