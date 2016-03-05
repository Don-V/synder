/** /todo figure out which of these I need */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#include "search.h"
#include "iv.h"

uint count_interval_overlaps_r(Interval *, struct IntervalTree *, uint);
uint count_point_overlaps_r(uint, struct IntervalTree *, uint);

IV * get_interval_overlaps_r(Interval *, struct IntervalTree *, IV *);
void get_point_overlaps_r(uint, struct IntervalTree *, IntervalResult *);

uint count_interval_overlaps(Interval * inv, struct IntervalTree * tree){
    return count_interval_overlaps_r(inv, tree, 0);
}

uint count_point_overlaps(uint point, struct IntervalTree * tree){
    return count_point_overlaps_r(point, tree, 0);
}

IntervalResult * init_IntervalResult(size_t size){
    IntervalResult * res = (IntervalResult *)malloc(sizeof(IntervalResult));
    res->iv = iv_init(size);
    res->inbetween = false;
    return(res);
}

void free_IntervalResult(IntervalResult * res){
    if(res){
        if(res->iv)
            iv_free(res->iv);
        free(res);
    }
}

IntervalResult * get_point_overlaps(uint point, struct IntervalTree * tree){
    IntervalResult * res = init_IntervalResult(IV_INITIAL_SIZE);
    get_point_overlaps_r(point, tree, res);
    return res;
}

IntervalResult * get_interval_overlaps(Interval * inv, struct IntervalTree * tree){
    IntervalResult * res = init_IntervalResult(IV_INITIAL_SIZE);
    res->iv = get_interval_overlaps_r(inv, tree, iv_init(8));
    res->inbetween = (res->iv->size == 1 && interval_overlap(inv, &res->iv->v[0]) != 1);
    return res;
}



uint count_point_overlaps_r(uint point, struct IntervalTree * tree, uint count){
    if(point >= tree->center) {
        for(int i = T_SIZE(tree) - 1; i >= 0 ; i--){
            if(point <= T_STOP_STOP(tree, i)){
                count++;
            } else {
                break;
            }
        }
        if(RIGHT(tree))
            return count_point_overlaps_r(point, RIGHT(tree), count);
    }
    else {
        for(int i = 0; i < T_SIZE(tree); i++){
            if(point >= T_START_START(tree, i)){
                count++;
            } else {
                break;
            }
        }
        if(LEFT(tree))
            return count_point_overlaps_r(point, LEFT(tree), count);
    }
    return count;
}

uint count_interval_overlaps_r(Interval * inv, struct IntervalTree * tree, uint count){
    if(!tree)
        return count;

    switch(point_overlap(tree->center, inv)){
    case lo:
        for(int i = T_SIZE(tree) - 1; i >= 0 ; i--){
            if(inv->start <= T_STOP_STOP(tree, i)){
                count++;
            } else {
                break;
            }
        }
        return count_interval_overlaps_r(inv, RIGHT(tree), count);
    case hi:
        for(int i = 0; i < T_SIZE(tree); i++){
            if(inv->stop >= T_START_START(tree, i)){
                count++;
            } else {
                break;
            }
        }
        return count_interval_overlaps_r(inv, LEFT(tree), count);
    default:
        count += T_SIZE(tree);
        return count_interval_overlaps_r(inv, RIGHT(tree),
               count_interval_overlaps_r(inv, LEFT(tree), count));
    }
}

void get_point_overlaps_r(uint point, struct IntervalTree * tree, IntervalResult * results){
    if(point >= tree->center) {
        for(int i = T_SIZE(tree) - 1; i >= 0 ; i--){
            if(point <= T_STOP_STOP(tree, i)){
                iv_add(results->iv, T_STOP(tree, i));
            } else {
                break;
            }
        }
        if(RIGHT(tree)){
            get_point_overlaps_r(point, RIGHT(tree), results);
        } 
        else if(!R_SIZE(results)) {
            results->inbetween = true;
            iv_add(results->iv, LAST_STOP(tree));
        }
    }
    else {
        for(int i = 0; i < T_SIZE(tree); i++){
            if(point >= T_START_START(tree, i)){
                iv_add(results->iv, T_START(tree, i));
            } else {
                break;
            }
        }
        if(LEFT(tree))
            get_point_overlaps_r(point, LEFT(tree), results);
        else if(!R_SIZE(results)) {
            results->inbetween = true;
            iv_add(results->iv, FIRST_START(tree));
        }
    }
}

IV * get_interval_overlaps_r(Interval * inv, struct IntervalTree * tree, IV * results){

    if(!tree)
        return results;

    switch(point_overlap(tree->center, inv)){
    case lo:
        for(int i = T_SIZE(tree) - 1; i >= 0 ; i--){
            if(inv->start <= T_STOP_STOP(tree, i)){
                iv_add(results, T_STOP(tree, i));
            } else {
                break;
            }
        }
        if(!RIGHT(tree) && !results->size){
            iv_add(results, LAST_STOP(tree));
        }
        return get_interval_overlaps_r(inv, RIGHT(tree), results);
    case hi:
        for(int i = 0; i < T_SIZE(tree); i++){
            if(inv->stop >= T_START_START(tree, i)){
                iv_add(results, T_START(tree, i));
            } else {
                break;
            }
        }
        if(!LEFT(tree) && !results->size){
            iv_add(results, FIRST_START(tree));
        }
        return get_interval_overlaps_r(inv, LEFT(tree), results);
    default: // in
        iv_join(results, tree->by_start);
        return get_interval_overlaps_r(inv, RIGHT(tree), 
               get_interval_overlaps_r(inv, LEFT(tree), results));
    };
}

bool test_search(){
    IntervalResult * res;
    IA * ia = init_set_ia(2);
    Interval a = {.start=10, .stop=20};
    Interval b = {.start=30, .stop=40};
    ia->v[0] = a;
    ia->v[1] = b;
    struct IntervalTree * tree = build_tree(ia);

    printf("search: count point overlaps in itree\n");
    assert(count_point_overlaps(5,  tree) == 0);
    assert(count_point_overlaps(25, tree) == 0);
    assert(count_point_overlaps(45, tree) == 0);
    assert(count_point_overlaps(10, tree) == 1);
    assert(count_point_overlaps(11, tree) == 1);
    assert(count_point_overlaps(20, tree) == 1);

    printf("search: retrieve intervals overlapping itree\n");
    res = get_point_overlaps(5, tree);
    assert(res->inbetween == true);
    free_IntervalResult(res);

    res = get_point_overlaps(12, tree);
    assert(res->inbetween == false);
    assert(res->iv->v[0].start == 10);
    free_IntervalResult(res);

    res = get_point_overlaps(25, tree);
    assert(res->inbetween == true);
    free_IntervalResult(res);

    Interval c = {.start=1,  .stop=5 };
    Interval d = {.start=5,  .stop=15};
    Interval e = {.start=22, .stop=28};

    printf("search: count interval overlaps in itree\n");
    assert(count_interval_overlaps(&c, tree) == 0);
    assert(count_interval_overlaps(&d, tree) == 1);
    assert(count_interval_overlaps(&e, tree) == 0);

    printf("search: retrieve intervals overlapping itree\n");
    res = get_interval_overlaps(&c, tree);
    assert(res->inbetween == true);
    free_IntervalResult(res);

    res = get_interval_overlaps(&d, tree);
    assert(res->inbetween == false);
    assert(res->iv->v[0].start == 10);
    free_IntervalResult(res);

    res = get_interval_overlaps(&e, tree);
    assert(res->inbetween == true);
    free_IntervalResult(res);

    free_interval_tree(tree);
    return true;
}
