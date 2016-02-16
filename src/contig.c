#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include "contig.h"
#include "block.h"

#include "itree/itree.h"
#include "itree/search.h"

Contig * init_contig(char * name, size_t size){
    Contig* con = (Contig*)malloc(sizeof(Contig));
    con->name = strdup(name);
    con->size = size;
    con->itree = NULL;
    con->block = (Block**)malloc(size * sizeof(Block*));
    con->by_stop = NULL;
    con->start_sorted = false;
    con->stop_sorted  = false;
    return con;
}

void free_contig(Contig * contig){
    if(contig){
        if(contig->itree)
            free_interval_tree(contig->itree);
        for(int i = 0; i < contig->size; i++){
            if(contig->block[i])
                free_block(contig->block[i]);
        }
        if(contig->by_stop){
            free(contig->by_stop);
        }
        free(contig->block);
        free(contig->name);
        free(contig);
    }
}

void print_contig(Contig * contig){
    printf("%lu\t%s\n", contig->size, contig->name);
    for(int i = 0; i < contig->size; i++){
        print_block(contig->block[i]);
    }
}

uint anchor(Contig * contig, uint x){
    Block ** blks = contig->block;
    uint N = contig->size;
    uint lo = 0;
    uint hi = N - 1;
    uint i = hi / 2;
    while(true){
        if(x >= blks[i]->start){
            if(i == (N-1) || x < blks[i+1]->start)
                return i;
            lo = i;
            i = (i + hi) / 2 + 1;
        }
        else{
            if(i == 0 || x > blks[i-1]->start)
                return i == 0 ? i : i-1;
            hi = i;
            i = lo + (i - lo) / 2;
        }
    }
    return i;
}

// local function
IA * ia_from_blocks(Contig * con){
    IA * ia = init_set_ia(con->size);
    for(int i = 0; i < con->size; i++){
        ia->v[i].start = con->block[i]->start;
        ia->v[i].stop = con->block[i]->stop;
        ia->v[i].link = (void*)con->block[i];
    }
    return ia;
}

Contig * get_region(Contig * con, uint a, uint b){
    if(!con->itree)
        con->itree = build_tree(ia_from_blocks(con));
    Interval inv = {.start = a, .stop = b}; 
    IntervalResult * res = get_interval_overlaps(&inv, con->itree);
    Contig * newcon;
    if(res->inbetween){
        // Only one adjacent region should be returned in the inbetween case
        assert(res->iv->size == 1); 
        newcon = init_contig(con->name, 2);
        Block * nearest = GET_RESULT_BLOCK(res, 0);
        if(nearest->stop < a){
            newcon->block[0] = nearest; 
            newcon->block[1] = NEXT_BLOCK_BYSTART(con, newcon->block[0]);
        } else {
            newcon->block[1] = nearest; 
            newcon->block[0] = PREV_BLOCK_BYSTOP(con, newcon->block[1]);
        }
    }
    else {
        newcon = init_contig(con->name, res->iv->size);
        for(int i = 0; i < res->iv->size; i++){
            newcon->block[i] = GET_RESULT_BLOCK(res, i);
        }
    } 
    free_IntervalResult(res);
    return newcon;
}

uint count_overlaps(Contig * con, uint a, uint b){
    if(!con->itree)
        con->itree = build_tree(ia_from_blocks(con));
    Interval * inv = init_interval(a, b); 
    uint count = count_interval_overlaps(inv, con->itree);
    free(inv);
    return count;
}

/** \todo check sort_contig_by_start function */
void sort_blocks_by_start(Contig * contig){
    if(!contig->start_sorted){
        qsort(contig->block, contig->size, sizeof(Block*), block_cmp_start);
        for(int i = 0; i < contig->size; i++){
            contig->block[i]->startid = i;
        }
        contig->start_sorted = true;
    }
}

/** \todo check sort_contig_by_stop function */
void sort_blocks_by_stop(Contig * contig){
    if(!contig->stop_sorted){
        if(!contig->by_stop){
            contig->by_stop = (Block**)malloc(contig->size * sizeof(Block*));
            memcpy(contig->by_stop, contig->block, contig->size * sizeof(Block*));
        }
        qsort(contig->block, contig->size, sizeof(Block*), block_cmp_stop);
        for(int i = 0; i < contig->size; i++){
            contig->block[i]->stopid = i;
        }
        contig->stop_sorted = true;
    }
}
