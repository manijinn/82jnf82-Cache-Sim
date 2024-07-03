# 82jnf82-Cache-Sim
My cache simulator.
A Cache Simulator that I developed. It can operate with LRU or FIFO and even utilize write-through or write-back. It can handle an input of addresses of 10+ million.

Compile and Run:
-The command for compilation is standard for C++:
g++ -o <name> simulator.cpp

-The command for running the executable is the same as specified in the project instructions. 
(Or in other words, the standard for C++).

./<name> <CACHE_SIZE> <ASSOCIATIVITY> <REPLACEMENT> <WRITE-BACK> <FILENAME/FILEPATH>
 
Example:./a 131072 4 1 1 MINIFE.t 
