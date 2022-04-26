#include "analyzer.h"

#include <utility>
#include <iostream>
#include <map>
#include <cassert>

// Globals used by CallStack class
std::map<uint64_t, BBInfo *> basicBlocks;
std::map<uint64_t, LoopInfo *> loops;

// Global variable stack
CallStack stack;

// BBpath buffer
std::vector<BBPathInfo *> bbpath_buffer;

bool enable_dbg_print = false;

static BBInfo *get_bbinfo(uint64_t addr) {
    if (basicBlocks.find(addr) != basicBlocks.end()) {
        return basicBlocks[addr];
    } else {
        // Middle of a BB
        for (auto bit : basicBlocks) {
            uint64_t head = bit.second->instructions[0];
            uint64_t tail = bit.second->instructions.back();
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
        exit(1);
    }
}


void CallStack::popFrame() {
    assert(!callStack.empty());

    StackFrame *frame = callStack.back();
    std::vector<uint64_t> inners;

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
    for (uint64_t l : inners) {
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
        exit(1);
    }
    
    frame->retaddr = last_call_retaddr;
    callStack.push_back(frame);
}

void CallStack::adjustCallStack(uint64_t bbhead) {
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

BBPathInfo *CallStack::isInPath(uint64_t bbhead) {
    StackFrame *frame = getTopFrame();
    
    // Search from back
    for (int i = frame->path.size() - 1; i >=0; i--) {
        // std::cerr << "DBG: " << i << ": "<< frame.path[i]->head << '\n';
        if (frame->path[i]->head == bbhead) {
            return frame->path[i];
        } else if (frame->path[i]->head < bbhead) {
            // Get the address of the last instruction
            uint64_t tail = basicBlocks[frame->path[i]->head]->instructions.back(); 
            if (tail && tail > bbhead) {
                return frame->path[i];
            }
        }
    }
    return nullptr;
}

void CallStack::newBB(uint64_t head) {
    BBPathInfo *bpi = new BBPathInfo();
    bpi->head = head;

    StackFrame *frame = getTopFrame();
    frame->path.push_back(bpi);
}

void CallStack::printCallStack(bool printBB=false) {
    std::cerr << "Printing current Call Stack: " << std::endl;
    for (int j = callStack.size() - 1; j >=0; j--) {
        uint64_t retaddr = callStack[j]->retaddr;
        uint64_t frameaddr = callStack[j]->path[0]->head;
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

        uint64_t startingAddress = frame->path[i]->head;
        BBInfo *bbinfo_current = basicBlocks[startingAddress];

        // Associate BB
        bbinfo_current->associatedTopLoop = curloop->head;

        // Nested loop cases: bbl is already a head for another loop L; This means L is a child of current loop.
        if (loops.find(bbinfo_current->head) != loops.end()) {
            LoopInfo *childLoop = loops[bbinfo_current->head];
            curloop->children.insert(childLoop);
        }

        for (uint64_t insaddr : bbinfo_current->instructions) {
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



void processBB(uint64_t bbhead) {
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

void linkInnerLoop() {
    // Perform inner loop linking
    for (auto bbit : basicBlocks) {
        BBInfo *bbinfo = bbit.second;
        if (!bbinfo->innerLoops.empty() && bbinfo->associatedTopLoop) {
            for (uint64_t l : bbinfo->innerLoops) {
               loops[bbinfo->associatedTopLoop]->children.insert(loops[l]); 
            }
        }
    }
}

void WrapUp() {
    linkInnerLoop();
    
    std::cerr << "################ loop Results ##################" << std::endl;
    
    std::map<uint64_t, uint64_t> address2loopid;
    for (auto loop : loops) {

        cout<<"Loop Head Basic Block: 0x"<< std::hex <<loop.second->head<<endl;
        
        for (auto I : loop.second->associatedInsts) {
            address2loopid[I] = loop.second->head;
        }
    }
}


std::vector<std::string> split_string(std::string s, const std::string& delimiter) {
    size_t pos = 0;
    std::string token;
    std::vector<std::string> ret;

    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        ret.push_back(token);
        s.erase(0, pos + delimiter.length());
    }
    if (s != "")
        ret.push_back(s);

    return ret;
}


void load_record(std::string record_filename) {
    auto ifs = std::ifstream(record_filename);
    std::string buf;

    while (ifs.good()) {
        std::getline(ifs, buf);
        if (buf == "0") {
            // BBInfo: load to basicBlocks
            std::getline(ifs, buf);
            // std::cerr << buf << endl;
            auto contents = split_string(buf, ",");

            BBInfo *bbinfo = new BBInfo;
            bbinfo->head = std::stoul(contents[0], nullptr, 16);
            bbinfo->contains_call = std::stoi(contents[1]);
            bbinfo->contains_ret = std::stoi(contents[2]);
            bbinfo->retaddr = std::stoul(contents[3], nullptr, 16);

            for (size_t i=4; i < contents.size(); i++) {
                bbinfo->instructions.push_back(std::stoul(contents[i], nullptr, 16));
            }

            basicBlocks[bbinfo->head] = bbinfo;

            continue; // next record
        } else if (buf == "1") {
            // BBPathInfo: load to bbpath_buffer
            std::getline(ifs, buf);
            auto contents = split_string(buf, ",");
            assert(contents.size() == 1);

            BBPathInfo *bpi = new BBPathInfo;
            bpi->head = std::stoul(contents[0], nullptr, 16);

            bbpath_buffer.push_back(bpi);

            continue; // next record
        }
    }

}



int main(int argc, char * argv[]){
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " " << "<loop detecct record file>" << endl;
        exit(1);
    }

    load_record(argv[1]);

    for (auto bpi : bbpath_buffer) {
        // std::cerr << "Process BB: 0x" << std::hex << bpi->head << std::endl;
        processBB(bpi->head);
    }

    WrapUp();

    return 0;
}
