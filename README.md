# LoopDetect: A binary loop detection tool based on Intel PIN

This tool is used to detect loops in binary programs. 

## Here are some important notices:

1. The detection is at basic block level, and the detected loop head is the address of the first basic block of the loop, which is determined during the instrumentation. The loop head is NOT the address where the jmp instruction jumped back to.
2. The function stack frame is tracked using the call - ret pair. The messy stack frame produced by dynamic linker (i.e. before the program stack has been constructed) will be omitted.

To be continued...