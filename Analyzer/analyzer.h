#ifndef LOOPDETECT_ANALYZER_H
#define LOOPDETECT_ANALYZER_H

#include <vector>
#include <set>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <utility>
#include <string>

using std::string;
using std::ofstream;
using std::cout;
using std::endl;

struct LoopInfo {
    uint64_t head;
    uint64_t iter = 1;
    uint64_t level;
    std::set<uint64_t> associatedInsts;
    std::set<LoopInfo *> children;
};

struct BBPathInfo {
    uint64_t head;
};

struct BBInfo {
    uint64_t head = 0; //Address of the BB/first instruction
    uint64_t iter = 1;
    bool contains_call = false;
    bool contains_ret = false;
    uint64_t retaddr = 0; // Only nonzero if contains call

    std::set<BBInfo *> loopEdges; // Backedge to loop head, if this BB is an end of the loop
    std::vector<uint64_t> instructions;
    uint64_t associatedTopLoop = 0; // The most "outer"/"parent" loop associated to this BB. TODO: Possible Bugs here
    std::set<uint64_t> innerLoops;
};

struct StackFrame {
    uint64_t retaddr; // Address to continue execution after return (this frame popped)
    
    std::vector<BBPathInfo *> path;
    std::set<uint64_t> topLoops;

    void popBB();
};


class CallStack {
    bool called = false; // Last processed BB contains call
    uint64_t last_call_retaddr = 0;
    bool returned = false; // Last processed BB contains return

public:
    std::vector<StackFrame *> callStack;
    void popFrame();
    void popBB();

    void pushBB(BBPathInfo *bpi);
    void printCallStack(bool printBB);

    void newFrame();
    void newBB(uint64_t head);

    StackFrame *getTopFrame();
    BBPathInfo *isInPath(uint64_t bbhead);

    void adjustCallStack(uint64_t bbhead);
};


#endif /* LOOPDETECT_ANALYZER_H */