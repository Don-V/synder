#include <stdlib.h>

#include "interval.h"

int cmp_stop(const void *ap, const void *bp){
    Interval a = * (Interval *) ap;
    Interval b = * (Interval *) bp;
    return((a.stop > b.stop) - (b.stop > a.stop));
}

int cmp_start(const void *ap, const void *bp){
    Interval a = * (Interval *) ap;
    Interval b = * (Interval *) bp;
    return((a.start > b.start) - (b.start > a.start));
}

Pos point_overlap(unsigned int a, Interval b){
    if(a < b.start){
        return lo;
    }
    else if(a > b.stop){
        return hi;
    }
    else{
        return in;
    }
}

Pos interval_overlap(Interval a, Interval b){
    if(a.stop < b.start){
        return lo;
    }
    else if(a.start > b.stop){
        return hi;
    }
    else{
        return in;
    }
}
