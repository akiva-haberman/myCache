#define _GNU_SOURCE
#include <ctype.h>
#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
// DEBUG: in (2,3,3) -> (73) HIT should be MISS, EVICTION
#include "cachelab.h"
unsigned int size;
typedef struct split_address {
    long tag;
    long blockIndex;
    long setIndex;
} split_address;

typedef struct line {
    int valid;
    long tag;
    int LRUT;
} line;

typedef struct cache {
    unsigned int numSets, E;
    unsigned int hitCount, missCount, evictionCount;
    unsigned int LRUT;
    line **line_mat;
} cache;

// -~-~- PRINT CACHE -~-~-
void print_cache(cache *cache) {
    // on the corners write set and cache line #
    // print valid, tag, LRU
    for (int i = 0; i < cache->numSets; i++) {
        printf("%d\n", i);
        for (int j = 0; j < cache->E; j++) {
            printf("| v:%d t:%lx LRU:%d | ", cache->line_mat[i][j].valid, cache->line_mat[i][j].tag, cache->line_mat[i][j].LRUT);
        }
        printf("\n");
    }
}

void print_cache_line(cache *cache, int currSet, int i) {
    printf("| v:%d t:%lx LRU:%d | ",
           cache->line_mat[currSet][i].valid,
           cache->line_mat[currSet][i].tag,
           cache->line_mat[currSet][i].LRUT);
}

void missOrHit(split_address spltAddr, cache *cache) {
    int currSet = spltAddr.setIndex;  // record the set to which
                                      // this address belongs
                                      // printf("currSet: %d\n", currSet);
    cache->LRUT++;                    // time moves on
    // First, check if there is a valid line with a tag match
    for (int i = 0; i < cache->E; i++) {
        if ((cache->line_mat[currSet][i].valid == 1) &&
            (spltAddr.tag == cache->line_mat[currSet][i].tag)) {
            print_cache_line(cache, currSet, i);
            printf("(%d) LINE %d HIT\n", cache->LRUT, i);
            print_cache_line(cache, currSet, i);
            printf("\n\n");
            cache->hitCount++;                               // Match found -> increment hitCount
            cache->line_mat[currSet][i].LRUT = cache->LRUT;  // record time
            return;
        }
    }
    // Next, check if there are any invalid lines
    for (int i = 0; i < cache->E; i++) {
        if (cache->line_mat[currSet][i].valid == 0) {
            print_cache_line(cache, currSet, i);
            printf("(%d) LINE %d MISS\n", cache->LRUT, i);
            cache->missCount++;                              // cold miss -> increment miss count
            cache->line_mat[currSet][i].tag = spltAddr.tag;  // fill open line
            cache->line_mat[currSet][i].valid = 1;           // validate line
            cache->line_mat[currSet][i].LRUT = cache->LRUT;  // record time
            print_cache_line(cache, currSet, i);
            printf("\n\n");
            return;
        }
    }
    // At this point, we know there is a capacity miss
    // There must be a miss followed by an eviction
    cache->missCount++;
    cache->evictionCount++;
    int currLRU = 0;  // initialize LRU line to line 0 in the set
    // Loop and set last used line to LRU
    for (int i = 1; i < cache->E; i++) {
        if (cache->line_mat[currSet][currLRU].LRUT >
            cache->line_mat[currSet][i].LRUT) {
            currLRU = i;
        }
    }
    // After the above loop, currLRU holds the oldest line ID
    print_cache_line(cache, currSet, currLRU);
    printf("(%d) LINE %d MISS, EVICTION\n", cache->LRUT, currLRU);
    cache->line_mat[currSet][currLRU].LRUT = cache->LRUT;
    cache->line_mat[currSet][currLRU].tag = spltAddr.tag;
    print_cache_line(cache, currSet, currLRU);
    printf("\n\n");
    return;
}

// -~-~- INIT CACHE -~-~-
void init_cache(cache *res, int s, int E) {
    res->numSets = (1 << s);
    res->E = E;
    res->hitCount = 0;
    res->missCount = 0;
    res->evictionCount = 0;
    res->LRUT = 0;
    res->line_mat = (line **)malloc(sizeof(line *) * res->numSets);
    if (!res->line_mat) {
        fprintf(stderr, "No memory :(\n");
        exit(1);
    }
    for (int q = 0; q < res->numSets; q++) {
        res->line_mat[q] = (line *)malloc(sizeof(line) * E);
        if (!res->line_mat[q]) {
            fprintf(stderr, "No memory :(\n");
            exit(1);
        }
        for (int j = 0; j < E; j++) {
            res->line_mat[q][j].valid = 0;
            res->line_mat[q][j].LRUT = 0;
        }
    }
    return;
}

// -~-~- FREE CACHE -~-~-
void free_cache(cache *cache) {
    for (int q = 0; q < cache->numSets; q++) {
        free(cache->line_mat[q]);
    }
    free(cache->line_mat);
    // free(cache);
}

// -~-~- ADDRESS SPLITTER -~-~-
void address_splitter(long address, int s, int b, split_address *spltAddr) {
    spltAddr->blockIndex = address & ((1 << b) - 1);
    spltAddr->setIndex = (address >> b) & ((1 << s) - 1);
    spltAddr->tag = (address >> (b + s));
    return;
}

// -~-~- MAIN -~-~-
int main(int argc, char *argv[]) {
    long address;
    char operation;
    unsigned int s;  // 2^s = number of sets
    unsigned int E;  // number of lines per set
    unsigned int b;  // 2^b is block size
    unsigned int help_flag = 0, verbose_flag = 0;
    split_address spltAddr;
    size_t len = 64;       // length of the char array, this is the
                           // expected upper bound on length
    size_t numCharacters;  // used to read line this is how many char
                           // is in the string
    int option_val = 0;
    FILE *file;
    char *string = (char *)malloc(len);
    while ((option_val = getopt(argc, argv, ":hvs:E:b:t:")) != -1) {
        switch (option_val) {
            case 'h':
                help_flag = 1;
                break;
            case 'v':
                verbose_flag = 1;
                break;
            case 's':
                s = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 't':
                file = fopen(optarg, "r");
                break;
        }
    }
    printf("s = %u, E = %u, b = %u, h = %u, v = %u\n", s, E, b, help_flag,
           verbose_flag);
    char **string_pointer = &string;
    cache cache;
    init_cache(&cache, s, E);
    if (string == NULL) {
        fprintf(stderr, "Could not allocate memory\n");
        exit(1);
    } else {
        while ((numCharacters = getline(string_pointer, &len, file)) != -1) {
            sscanf(string, " %c %lx,%d", &operation, &address, &size);
            // printf("operation = %d, address = %lx, size = %d\n", (char)operation, address, size);
            //  printf("s = %d, b = %d\n", s, b);
            address_splitter(address, s, b, &spltAddr);
            if ((operation == 'L') || (operation == 'S')) {
                printf("%lx : set %lx, operation: %c\n", address, spltAddr.setIndex,operation);
                missOrHit(spltAddr, &cache);
            } else if (operation == 'M') {
                printf("%lx : set %lx, operation: %c\n", address, spltAddr.setIndex,operation);
                missOrHit(spltAddr, &cache);
                missOrHit(spltAddr, &cache);
            }
        }
        // printf("\n");
    }
    // print_cache(&cache);
    free_cache(&cache);
    free(string);
    fclose(file);
    printSummary(cache.hitCount, cache.missCount, cache.evictionCount);
    return 0;
}