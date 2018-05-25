#!/bin/bash
#opti - executable name
#programs/Tiny/BP_8_128/BP_8_128 - program prefix, the code expects to find all the files it needs, such as programs/Tiny/BP_8_128/BP_8_128.in inside that folder
#2 - machine rows
#4 - machine cols; this is a 2x4 system
#0 - fixed option, always use 0
#2 - routing using one-bend paths (use 1 for rectangle reservation)
#20000 - upper bound for binary search

./opti programs/Tiny/BP_8_128/BP_8_128 2 4 0 2 20000 
