#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define BLOCK_SIZE 64
#define MAX_LINE_SIZE 21

#define FLAG bool
#define COUNTER unsigned long
#define ENTRY unsigned long long

//----- Global Variables for use across functions -----
FLAG lru, write_back, **dirty;
COUNTER num_sets, assoc, hit, miss, mem_write, mem_read, op_id, *fifo_position, **lru_position;
ENTRY **tag_array;

void simulate_access(char op, ENTRY add) {
    ENTRY add_msb = add / BLOCK_SIZE;
    ENTRY tag = add_msb / num_sets;
    COUNTER set = add_msb % num_sets;
    COUNTER position = 0;
    FLAG found = 0;

    //Try to find the tag in cache
    for(COUNTER i=0; i<assoc; i++) {
        if (tag == tag_array[set][i]) {
            position = i;
            found = 1;
            break;
        }
    }

    if (found) {
        hit++;
    } else {
        miss++;
        //Obtain the new position in cache based on caching policy
        if(lru) {
            COUNTER min_op_id = lru_position[set][0];
            position = 0;
            for (COUNTER i=1; i<assoc; i++) {
                if (min_op_id > lru_position[set][i]) {
                    min_op_id = lru_position[set][i];
                    position = i;
                }
            }
        } else {
            position = fifo_position[set]++;
            fifo_position[set] %= assoc;
        }
        //If the current cache position has dirty write then write to memory
        if (write_back && dirty[set][position]) {
            mem_write++;
            dirty[set][position] = 0;
        }
        //Check if we need to read from main memory
        // if (op == 'R') {
            mem_read++;
        // }
        //Update the cache with new data
        tag_array[set][position] = tag;
    }

    //For LRU caching, update the recency information
    if (lru) {
        lru_position[set][position] = ++op_id;
    }

    //If it's a cache write operation
    if (op == 'W') {        
        if (write_back) {
            //Set the cache as dirty if written to cache
            dirty[set][position] = 1;
        } else {
            //Otherwise write through to the memory
            mem_write++;
        }
    }
}

int main(int argc, char** argv) {
    char op, buffer[MAX_LINE_SIZE];
    COUNTER add, cache_size;
    FILE* inp;

    if (argc != 6) {
        printf("Usage: ./SIM <CACHE_SIZE> <ASSOC> <REPLACEMENT> <WB> <TRACE_FILE>\n");
        return -1;
    }

    //Set configuration parameters based on command line parameters
    cache_size = atol(argv[1]);
    assoc = atoi(argv[2]);
    lru = !atoi(argv[3]);
    write_back = atoi(argv[4]);
    num_sets = cache_size / (BLOCK_SIZE * assoc);

    //Open the trace file for input
    inp = fopen(argv[5], "r");
    if (!inp) {
        printf("ERROR: Failed to open the specified file: %s\n", argv[5]);
        return -1;
    }

    //Allocate the memory to cache data strctures
    tag_array = (ENTRY**) calloc(num_sets, sizeof(ENTRY*));
    if (lru) {
        lru_position = (COUNTER**) calloc(num_sets, sizeof(COUNTER*));
    } else {
        fifo_position = (COUNTER*) calloc(num_sets, sizeof(COUNTER));
    }
    if (write_back) {
        dirty = (FLAG**) calloc(num_sets, sizeof(FLAG*));
    }
    for (COUNTER i=0; i<num_sets; i++) {
        tag_array[i] = (ENTRY*) calloc(assoc, sizeof(ENTRY));
        if (lru) {
            lru_position[i] = (COUNTER*) calloc(assoc, sizeof(COUNTER));
        }
        if (write_back) {
            dirty[i] = (FLAG*) calloc(assoc, sizeof(FLAG));
        }
    }

    //Process trace file line-by-line
    while(fgets(buffer, MAX_LINE_SIZE, inp)) {
        sscanf(buffer, "%c %lx", &op, &add);
        simulate_access(op, add);
    }

    //Close the trace file
    fclose(inp);

    //Release the allocated memory
    for (COUNTER i=0; i<num_sets; i++) {
        free(tag_array[i]);
        if (lru) {
            free(lru_position[i]);
        }
        if (write_back) {
            free(dirty[i]);
        }
    }
    free(tag_array);
    if (lru) {
        free(lru_position);
    } else {
        free(fifo_position);
    }
    if (write_back) {
        free(dirty);
    }

    //print out the statistics
    printf("Total miss ratio for L1 cache: %f\n", (double) miss / (hit + miss));
    printf("# writes to memory: %lu\n", mem_write);
    printf("# reads from memory: %lu\n", mem_read);

    return 0;
}