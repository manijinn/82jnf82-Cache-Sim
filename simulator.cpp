#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <cmath>
using namespace std;


typedef struct Element {
    Element *above;
    Element *below;
    unsigned long long int address;
    unsigned long long int tag;
    int dirty;

    Element() {
        above = nullptr;
        below = nullptr;
        address = 0;
        tag = 0;
        dirty = 0;
    }
} Element;


// The stack will be organized such that {In FIFO: the top is the oldest, bottom the newest | In LRU: the top is the MRU, the bottom is the LRU}.
typedef struct Stack {
    Element *top;
    Element *bottom;
    int maxsize; // based on associativity
    int currentsize;

    Stack() {
        top = nullptr;
        bottom = nullptr;
        maxsize = 0;
        currentsize = 0;
    }
} Stack;


const unsigned long long int BLOCK_SIZE = 64;
unsigned long long int tagbits = 0;
unsigned long long int indexbits =0;
unsigned long long int offsetbits = 6; // Hardcoded offset size since we can assume for all scenarios block size will be 64 bytes. This is log 64 / log 2 by the way.
int num_sets = 0;
Stack *sets;


int w_miss = 0;
int r_miss = 0;
int hits = 0;
int reads = 0;
int writes = 0;
int accesses = 0;


static Stack initSet(Stack blocks, unsigned long long int address, unsigned long long int tag, int ASSOC) { 
    blocks.top = new Element;
    // At this point, the cache only has one element, so the top is also the bottom and most/least recently used.
    blocks.bottom = blocks.top;
    blocks.top->address = address;
    blocks.top->tag = tag;
    blocks.maxsize = ASSOC;
    blocks.currentsize = 1; // Add 1 since we are initializing the stack with one address.

    return blocks;
}


static void LRUInsertion(int index, Element *temp) {
    temp->below = sets[index].top;
    sets[index].top->above = temp;
    sets[index].top = temp;
}

static void FIFOInsertion(int index, Element *temp) {
    // Set temp's above pointer to point to the bottom of the stack (we are adding a new element to the bottom of the stack).
    temp->above = sets[index].bottom;
    // Make sure to set the bottom of the stack's below pointer to the newly added element.
    sets[index].bottom->below = temp;
    // Declare the newly added element as the new bottom element.
    sets[index].bottom = temp;
}


// Replacement/insertion handling
static void Replacement(int index, Element *temp, int REPLACEMENT, int WB) {
    // LRU
    if (REPLACEMENT == 0) {

        // Stack is full, perform LRU replacement.
        // Place temp on top. It is the MRU. Remove bottom, it is the LRU. Use temp again to store the 2nd-to-last element.
        if (sets[index].currentsize >= sets[index].maxsize) {
            LRUInsertion(index, temp);
            temp = sets[index].bottom->above;
            temp->below = nullptr;

            // Check if element is dirty.
            if (WB == 1 && sets[index].bottom->dirty == 1) {

                writes++;
            }

            delete sets[index].bottom;
            sets[index].bottom = temp;
        }

        // Stack in not full, insert element normally.
        else {
            LRUInsertion(index, temp);
            sets[index].currentsize++;
        }
    }

    // FIFO
    else {

        if (sets[index].currentsize >= sets[index].maxsize) {
            FIFOInsertion(index, temp);

            // Store the 2nd highest element into the temporary pointer.
            temp = sets[index].top->below;
            temp->above = nullptr;

            // Now we will soon delete the oldest element, which is stored at the top always. But first we check if it has a dirty bit and if we are using write-back.
            // If true, then we write to memory, increment writes.
            if (WB == 1 && sets[index].top->dirty == 1) {

                writes++;
            }

            // Delete the top element of the stack. temp is now the new de facto top of the stack.
            delete sets[index].top;
            // Officially declare temp as the new top.
            sets[index].top = temp;
        }

        else {
            FIFOInsertion(index, temp);
            // Increment current size of the stack.
            sets[index].currentsize++;
        }
    }
}


static void Push(char operation, unsigned long long int address, int ASSOC, int REPLACEMENT, int WB) {
    int index = (address / BLOCK_SIZE) % num_sets;
    unsigned long long int tag = address / BLOCK_SIZE;

    // Base case: If the set is empty, we automatically just insert a new block. Counts as a miss.
    if (sets[index].top == nullptr) {
        sets[index] = initSet(sets[index], address, tag, ASSOC);

        if (operation == 'R') {
            r_miss++;
            reads++;
        }

        else {
            w_miss++;
            reads++; // Per Dr.Suboh's example, writes also increment memory reads, if it's not already in the stack.

            // Write through.
            if (WB == 0) {
                writes++;
            }

            // Write back. 
            else {
                sets[index].top->dirty = 1;
                return;
            }
        }
        return;
    }

    // 2nd base case: Search the stack for the relevant tag/data.
    Element *traverse = sets[index].top;

    while (traverse != nullptr) {
        
        // Address with the same tag in already in the data.
        if (traverse->tag == tag) {
            hits++;

            // Write
            if (operation == 'W') {

                if (WB == 0) {
                    writes++;
                }

                // Write back, since the data is being accessed again, mark it as dirty.
                else {
                    traverse->dirty = 1;
                }
            }

            // If we are using LRU, then we have to update its position to the top of the stack, which symbolizes it as the most recently used address.
            if (REPLACEMENT == 0) {

                // If data is already at top (it is MRU), don't do anything.
                if (sets[index].top == traverse) {
                    return;
                }

                // Special case if data is the bottom. Needed since nullptrs can crash the program.
                else if (traverse == sets[index].bottom) {
                    // Modify the second-to-last element
                    Element *temp;
                    temp = traverse->above;
                    temp->below = nullptr;
                    sets[index].bottom = temp;

                    // Set the traversal element as the top element.
                    traverse->above = nullptr;
                    traverse->below = sets[index].top;
                    sets[index].top->above = traverse;
                    sets[index].top = traverse;
                }

                else {
                    // Overwrite pointers that point to traverse.
                    Element *temp_above = traverse->above;
                    Element *temp_below = traverse->below;
                    temp_above->below = temp_below;
                    temp_below->above = temp_above;

                    // Set traverse to the top of the stack.
                    traverse->above = nullptr;
                    traverse->below = sets[index].top;
                    sets[index].top->above = traverse;
                    sets[index].top = traverse;
                }
            }
            return;
        }
        traverse = traverse->below;
    }

    // Presumably, at this point we did not find the data in the cache.
    // Create new element.

    Element *temp = new Element;
    temp->address = address;
    temp->tag = tag;

    // Read operation.
    if (operation == 'R') {
        // We didn't find the tag in the stack, so now we have to read from memory. Increment reads misses.
        r_miss++;
        reads++;
        Replacement(index, temp, REPLACEMENT, WB);
    }

    // Write operation.
    else {
        // Increment write misses.
        w_miss++;
        reads++; // Per Dr.Suboh's example, writes also increment memory reads, if it's not already in the stack.

        // Write through. 
        if (WB == 0) {
            writes++;
            Replacement(index, temp, REPLACEMENT, WB);
        }

        // Write back.
        else {
            temp->dirty = 1;
            Replacement(index, temp, REPLACEMENT, WB);
        }
    }
}


static void initSets(int CACHE_SIZE, int ASSOC) {
    // Get the number of sets
    num_sets = CACHE_SIZE / (64 * ASSOC);
    // Create an array with the calculated number of sets
    sets = new Stack[num_sets];
}


int main(int argc, char* argv[]) {
    // Get command line arguments
    int CACHE_SIZE = atoi(argv[1]);
    int ASSOC = atoi(argv[2]);
    int REPLACEMENT = atoi(argv[3]);
    int WB = atoi(argv[4]);
    string TRACE_FILE = argv[5];

    // Initialize the set array
    initSets(CACHE_SIZE, ASSOC);

    // Get file and open it
    const char* charArray = TRACE_FILE.c_str();
    FILE * file = fopen(charArray, "r"); 

    char operation;
    unsigned long long int address;

    // Push each address to the cache.
    while (!feof(file)) {
        fscanf(file , " %c %llx" , &operation, &address);
        accesses++;

        Push(operation, address, ASSOC, REPLACEMENT, WB);
    }

    float miss_ratio = ((float)w_miss + float(r_miss)) / (float)accesses;

    printf("miss ratio: %f\n", miss_ratio);
    printf("writes: %d\nreads: %d", writes, reads);

    // Delete sets and stacks.
    Element *deletearray[ASSOC];

    for (int i = 0; i < num_sets; i++) {
        Element *temp = sets[i].top;

        while (temp != nullptr) {
            temp = temp->below;
            delete sets[i].top;
            sets[i].top = temp;
        }
    }

    delete[] sets;
    fclose(file);
}