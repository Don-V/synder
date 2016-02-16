#ifndef __INTERVAL_H__
#define __INTERVAL_H__

/** the eponymous structure */
typedef struct {
    unsigned int start;
    unsigned int stop;
    void * link; /* a pointer to arbitrary related data */
} Interval;

/** initialize Interval with start and stop, but set link to NULL */
Interval * init_interval(unsigned int start, unsigned int stop);

/** position of Interval or point A relative to B */
typedef enum {lo=0, in=1, hi=2} Pos;

/** compare intervals by stop */
int cmp_stop(const void *, const void *);

/** compare intervals by start */
int cmp_start(const void *, const void *);

/** find position of point A relative to interval B (see Pos) */
Pos point_overlap(unsigned int A, Interval * B);

/** find position of interval A relative to interval B (see Pos) */
Pos interval_overlap(Interval *, Interval *);

#endif
