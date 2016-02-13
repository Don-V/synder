#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "itree.h"
#include "interval.h"
#include "interval-tree.h"
#include "ia.h"
#include "iv.h"

/* local function prototypes */
void print_interval_tree_verbosity_1(struct IntervalTree * n, int depth, char pos);
void print_interval_tree_verbosity_2(struct IntervalTree * n, int depth, char pos);
void print_interval_tree_verbosity_3(struct IntervalTree * n, int depth, char pos);
void print_interval_tree_r(struct IntervalTree * n, int depth, char pos, int verbosity);
uint get_center(IA *);


struct IntervalTree * build_tree(IA * intervals){
    /* initialize returned product */
    struct IntervalTree * tree = init_interval_tree();

    tree->center = get_center(intervals);

    /* array to store position of center point relative to each interval
     * in intervals
     * lo = 0 -> center is lower than interval
     * in = 1 -> center is inside interval
     * hi = 2 -> center is higher than interval
     */
    Pos * pos = (Pos *)malloc(intervals->size * sizeof(Pos));

    /* count of intervals in each relative position */
    int npos[] = {0, 0, 0};

    /* iterate over intervals classifying and counting each */
    for(int i = 0; i < intervals->size; i++){
        pos[i] = point_overlap(tree->center, intervals->v[i]);  
        npos[pos[i]]++;
    }

    /* initialise interval arrays */
    IA * right    = init_set_ia(npos[lo]);
    IA * left     = init_set_ia(npos[hi]);
    tree->by_start = init_set_ia(npos[in]);
    tree->by_stop  = init_set_ia(npos[in]);

    /* track index of interval to add */
    size_t lo_idx = 0;
    size_t in_idx = 0;
    size_t hi_idx = 0;

    /* assign intervals to appropriate partitions */
    for(size_t i = 0; i < intervals->size; i++){
        switch(pos[i]){
            case lo:
                right->v[lo_idx] = intervals->v[i];
                lo_idx++;
                break;
            case in:
                tree->by_start->v[in_idx] = intervals->v[i];
                tree->by_stop->v[in_idx]  = intervals->v[i];
                in_idx++;
                break;
            case hi:
                left->v[hi_idx] = intervals->v[i];
                hi_idx++;
                break;
            default:
                fprintf(stderr, "ERROR: something really weird happened ... sorry\n");
                exit(EXIT_FAILURE);
        }
    }

    if(npos[lo] > 0){
        tree->r_child = build_tree(right);   
    } else {
        free_ia(right);
    }

    if(npos[hi] > 0){
        tree->l_child = build_tree(left);   
    } else {
        free_ia(left);
    }
    
    if(npos[in] > 0){
        qsort(&tree->by_start->v[0], npos[in], sizeof(Interval), cmp_start);
        qsort(&tree->by_stop->v[0],  npos[in], sizeof(Interval), cmp_stop);
    }

    free(pos);
    free_ia(intervals);

    return(tree);
}



/*
 * Select a point at the center of the middle interval.
 * This guarantees at least one interval overlaps each node.
 * If the intervals are sorted, it also favors (but doesn't guarantee) a
 * balanced tree.
 */
uint get_center(IA * intr){
    // get the central index
    size_t i = intr->size / 2;
    // get the center point on this index
    uint x = (intr->v[i].stop - intr->v[i].start) / 2 + intr->v[i].start;
    return x;
}



/* write tree and center */
void print_interval_tree_verbosity_1(struct IntervalTree * n, int depth, char pos){
    printf("%*d - %c%d\n", depth * 2, depth, pos, n->center);
}

/* write tree, center, and start-sorted */
void print_interval_tree_verbosity_2(struct IntervalTree * n, int depth, char pos){
    printf("%*d   %*s\t%c%d:",
           depth * 2, depth, 
           10 - depth * 2, "", pos, n->center);
    for(int i = 0; i < n->by_start->size; i++){
        printf("(%u,%u) ",
               n->by_start->v[i].start,
               n->by_start->v[i].stop);
    }
    printf("\n");
}

/* write start- and stop-sorted vectors for each node */
void print_interval_tree_verbosity_3(struct IntervalTree * n, int depth, char pos){
    print_interval_tree_verbosity_1(n, depth, pos);
    for(int i = 0; i < n->by_start->size; i++){
        printf("\t\t(%u,%u) ",
               n->by_start->v[i].start,
               n->by_start->v[i].stop);
        printf("(%u,%u)\n",
               n->by_stop->v[i].start,
               n->by_stop->v[i].stop);
    }
}

/* local print function */
void print_interval_tree_r(struct IntervalTree * n, int depth, char pos, int verbosity){
    switch(verbosity){
        case 1:
            print_interval_tree_verbosity_1(n, depth, pos); break;
        case 2:
            print_interval_tree_verbosity_2(n, depth, pos); break;
        case 3:
            print_interval_tree_verbosity_3(n, depth, pos); break;
        default:
            fprintf(stderr, "verbosity must be 1, 2, or 3\n");
            exit(EXIT_FAILURE);
    }
    depth++;
    if(n->l_child){
        print_interval_tree_r(n->l_child, depth, 'l', verbosity);
    }
    if(n->r_child){
        print_interval_tree_r(n->r_child, depth, 'r', verbosity);
    }
}

/* public wrapper for real print function */
void print_interval_tree(struct IntervalTree * n, int verbosity){
    print_interval_tree_r(n, 0, 'c', verbosity);
}
