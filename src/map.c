#include "map.h"
#include "contig.h"
#include "synmap.h"

// A local utility structure used filter and store contiguous sets
typedef struct CSList{
  struct CSList * next;
  Block * bound[2]; 
  ContiguousSet * cset;
} CSList;
CSList * init_empty_CSList();
CSList * init_CSList(Block * blk);
void add_blk_CSList(CSList * cslist, Block * blk);
void add_cset_CSList(CSList * cslist, ContiguousSet * cset, long bounds[2]);
void free_CSList(CSList * cslist);

typedef struct SI_Bound{
  long bound;
  int flag; 
} SI_Bound;

SI_Bound * get_si_bound(
  long q,
  long set_bounds[2],
  Block * blk_bounds[2],
  Direction d,
  bool inverted
);

int get_flag(SI_Bound * br[2]);

void find_search_intervals(Synmap * syn, FILE * intfile, bool pblock)
{

  // start and stop positions read from input line
  long bounds[2];
  // Max and min nodes in current contiguous set
  long set_bounds[2]; 
  // Max and min blocks retrieved from itree.
  Block * blk_bounds[2]; 
  // Search interval boundary information
  SI_Bound * bound_results[2];
  // List of contiguous sets
  CSList *cslist;
  // A pointer to the root node of cslist (needed only for freeing the list)
  CSList *root;
  // Does starget strand == '-'?
  bool inverted;
  // Name of query input (e.g. AT1G01010)
  char seqname[NAME_BUFFER_SIZE];
  // Index of query chromosome
  size_t chrid;
  // Row output of itree
  ResultContig * rc;
  // Row output of ctree
  ResultContig * crc;
  // Search interval score
  float score;

  char *line = (char *) malloc(LINE_BUFFER_SIZE * sizeof(char));
  while (fgets(line, LINE_BUFFER_SIZE, intfile) && !feof(intfile)) {
    if (!sscanf(line,
                "%zu %*s %*s %zu %zu %*s %*c %*s %s\n",
                &chrid, &bounds[LO], &bounds[HI], seqname))
    {
      printf("invalid input\n");
      exit(EXIT_FAILURE);
    }
    check_in_offset(bounds[LO], bounds[HI]);
    bounds[LO] -= global_in_start;
    bounds[HI] -= global_in_stop;

    rc = get_region(SGC(syn, 0, chrid), bounds[LO], bounds[HI], false);

    cslist = init_empty_CSList();
    root = cslist;

    // get list of highest and lowest members of each contiguous set
    for(size_t i = 0; i < rc->size; i++){
      add_blk_CSList(cslist, rc->block[i]); 
    }

    crc = get_region(SGC(syn, 0, chrid), bounds[LO], bounds[HI], true);
    if(! (crc->inbetween || crc->leftmost || crc->rightmost) ){
        for(size_t i = 0; i < crc->size; i++){
          add_cset_CSList(cslist, crc->cset[i], bounds); 
        }
    }

    // Iterate through each contiguous set, for each find the search interval
    // For each contiguous set, there is exactly one search interval.
    for(; cslist != NULL; cslist = cslist->next){

      set_bounds[LO] = get_set_bound(cslist->bound[LO], LO);
      set_bounds[HI] = get_set_bound(cslist->bound[HI], HI);

      blk_bounds[LO] = cslist->bound[LO];
      blk_bounds[HI] = cslist->bound[HI];

      inverted = blk_bounds[HI]->over->strand == '-';

      score = calculate_score(bounds[LO], bounds[HI], blk_bounds[LO]) +
              calculate_target_score(bounds[LO], bounds[HI], blk_bounds);

      bound_results[inverted ^ LO] =
        get_si_bound(bounds[LO], set_bounds, blk_bounds, LO, inverted);
      bound_results[inverted ^ HI] =
        get_si_bound(bounds[HI], set_bounds, blk_bounds, HI, inverted);


      printf("%s\t%s\t%zu\t%zu\t%s\t%zu\t%zu\t%c\t%f\t%i\t%i\t%i\n",
        seqname,
        blk_bounds[LO]->parent->name,
        bounds[LO] + global_out_start,
        bounds[HI] + global_out_stop,
        blk_bounds[LO]->over->parent->name,
        bound_results[LO]->bound + global_out_start,
        bound_results[HI]->bound + global_out_stop,
        blk_bounds[LO]->over->strand,
        score,
        bound_results[LO]->flag,
        bound_results[HI]->flag,
        rc->inbetween || rc->leftmost || rc->rightmost
      );

      free(bound_results[0]);
      free(bound_results[1]);

    }

    free_ResultContig(rc);
    free_CSList(root);
  }

  free(line);
}

SI_Bound * init_SI_Bound(long bound, int flag){
  SI_Bound * br = (SI_Bound *)malloc(sizeof(SI_Bound));
  br->bound = bound;
  br->flag = flag;
  return br;
}

SI_Bound * get_si_bound(
  long q,
  long set_bounds[2],
  Block * blk_bounds[2],
  Direction d,
  bool inverted)
{
  // Invert orientation mapping to target if search interval is inverted
  Direction vd = inverted ? !d : d;
  // See contiguous.h
  int flag = 0;
  // non-zero to ease debugging
  long bound = 444444;

  // All diagrams are shown for the d=HI case, take the mirror image fr d=LO.
  //
  // KEY:
  // |x--  --y| - bounds of the contiguous block; start==x, stop==y 
  // a========b - a syntenic block with start == a and stop == b
  //   <---q    - the query interval, with stop == q (start doesn't matter)
  // a==b--c==d - query bounding blocks in the contiguous set
  // [===]      - a non-bounding block in the same contiguous set
  //  ...  F=== - nearest non-adjacent block ***ON THE TARGET***, F=start
  //      ^     - search interval bound
  //
  // Possible snap positions (relative to query)
  //   |x...[===]-----a=======b-----c=======d-----[===]...y|  ...  F===
  //                 ^        ^    ^        ^    ^                ^

  // Positions of a, b, c, and d (as shown above)
  long pnt_a = blk_bounds[!d]->pos[!d];
  long pnt_b = blk_bounds[!d]->pos[ d];
  long pnt_c = blk_bounds[ d]->pos[!d];
  long pnt_d = blk_bounds[ d]->pos[ d];


  // This may occur when there is only one element in the ContiguousSet
  //          |x-----y|
  //       ...a=======b      
  //         ^
  //   <---q
  // q < x
  if(REL_LT(q, set_bounds[!d], d)){
    bound = blk_bounds[!d]->over->pos[!vd];
    flag = UNBOUND;
  }

  //   |x---[===]-------a=======b...y|   ...    F===
  //                   ^
  //              --q
  // q < a
  else if(REL_LT(q, pnt_a, d)){
    bound = blk_bounds[!d]->over->pos[!vd];
    flag = BOUND;
  }

  //   |x...a=======b...y|   ...    F===
  //                ^
  //        <---q
  // q < b
  else if(REL_LE(q, pnt_b, d)){
    bound = blk_bounds[!d]->over->pos[vd];
    flag = ANCHORED;
  }

  //   |x...a=======b-------c=======d...y|  ...  F===
  //                       ^
  //                  <--q
  // q < c && q > b
  //   (q > b test required since blk_bounds[LO] can equal blk_bounds[HI])
  else if(REL_LT(q, pnt_c, d) && REL_GT(q, pnt_b, d))
  {
    bound = blk_bounds[d]->over->pos[!vd];
    flag = BOUND;
  }

  //   |x...a=======b-------c=======d...y|  ...  F===
  //                                ^
  //             <--------------q
  // q < d
  else if(REL_LE(q, pnt_d, d)){
    bound = blk_bounds[d]->over->pos[vd];
    flag = ANCHORED;
  }

  //   |x...a=======b-------c=======d-------[===]...y|  ...  F===
  //                                       ^
  //              <----------------------q
  // q < y, (which implies there is a node after d)
  else if(REL_LE(q, set_bounds[d], d)){
    bound = blk_bounds[d]->cnr[d]->over->pos[!vd];
    flag = BOUND;
  }

  // If none of the above, the bound beyond anything in the contiguous set
  // In this case, the hi and lo Contiguous nodes will be the same
  else {
    // Get nearest non-overlapping sequence
    Block * downstream_blk = blk_bounds[d]->over->adj[vd];
 
    // adjacent block on TARGET side exists
    //    |x...--a=======b|
    //    |x...--c=======d|  ...  F===
    //                           ^
    //                    <---q
    if(downstream_blk != NULL){
      flag = UNBOUND;
      bound = downstream_blk->pos[!vd];
    }
    //    |x...--a=======b|
    //    |x...--c=======d|  ...  THE_END
    //                    <---q
    // query is further out than ANYTHING in the synteny map
    else {
      bound = vd ? blk_bounds[d]->over->parent->length - 1 : 0;
      flag = blk_bounds[d]->over->pos[vd] == bound ? BEYOND : EXTREME;
    }
  }

  return init_SI_Bound(bound, flag);
}


// Arguments:
//  near - the distance from the block to the expected near end of the query
//  far  - the distance from the block to the expected far end of the query
//               b1        b2  |  b1        b2
//      a1  a2   |=========|   |  |=========|    a1  a2
//      |===|    |             |                 |===|
//      |<------>| far         |       near |<-->|
//          |<-->| near        |        far |<------>|
float _flank_area(long near, long far, float k){

    // If far <= 0, this means there is no interval to score in this direction
    if(far > 0){
        // Adjust the near boundary, if needed, for example:
        //             b1        b2
        //             |=========| 
        //      a1       |   a2    
        //      |============|     
        //      |<------>|     far 
        //               |<->| near (negative value)
        //               |     snap near to relative 0
        near = near > 0 ? near : 0; 

        // the weight falls exponentially with distance from the query, e.g.
        // $$ \int_{near}^{far} exp(-kx) dx $$
        // which evaluates to the following:
        return (1 / k) * ( exp(-1 * k * near) - exp(-1 * k * far) );
    }
    return 0;
}

// This function is a wrapper for calculate_score. It maps the query position
// on the query scaffold to the most likely position in the given contiguous
// set.
float calculate_target_score(long a1, long a2, Block * bounds[2]){
    // This is a stub
    return 0; 
}

float calculate_score(long a1, long a2, Block * blk){

    float area;
    long b1, b2;
    float k = 0.001;
    float score = 0;

    if(blk == NULL)
        return score;

    // rewind
    while(blk->cnr[0] != NULL){
        blk = blk->cnr[0];
    }

    for(; blk != NULL ; blk = blk->cnr[1]){
        b1 = blk->pos[0];
        b2 = blk->pos[1];

        //               a1        a2
        //      b1  b2   |=========|    query interval
        //      |===|    |              syntenic interval
        //      |---|....|              interval to score
        // near difference := i1 = a1 - b2
        // far difference  := i2 = a1 - b1
        // i1 and i2 may be negative, if query is not in the above position
        area = _flank_area(a1 - b2, a1 - b1, k) +

        //    a1        a2
        //    |=========|    b1  b2     query interval
        //              |....|---|      syntenic interval
        //                   |===|      interval to score
        // near difference := i1 = b1 - a2
        // far difference  := i2 = b1 - a1
               _flank_area(b1 - a1, b2 - a1, k) +

        //         a1        a2
        //         |=========|         query interval
        //             |=====|=====|   syntenic interval
        //             b1    |     b1
        //             |-----|         overlapping interval
        //             i1    i2
               overlap_length_ll(a1, a2, b1, b2);

        // NOTE: I am kind of adding length to area here, but it actually
        // works. `_flank_area` returns the area of a segment of the base
        // exponentional (i.e. where f(0) = 1). Multiplying this base
        // exponential area by the syntenic link score, gives the final score
        // for the non-overlapping segment. In the same way, multiplying the
        // overlapping segment length by the score, gives the overlapping
        // segmental score.

        score += blk->score * area;
    }
    return score;
}


CSList * init_empty_CSList(){
  CSList * cslist   = (CSList *)malloc(sizeof(CSList));
  cslist->next      = NULL;
  cslist->bound[LO] = NULL;
  cslist->bound[HI] = NULL;
  cslist->cset      = NULL;
  return(cslist);
}

CSList * init_CSList(Block * blk){
  CSList * cslist = (CSList *)malloc(sizeof(CSList));
  cslist->next = NULL;
  cslist->bound[LO] = blk;
  cslist->bound[HI] = blk;
  cslist->cset = blk->cset;
  return(cslist);
}

void add_cset_CSList(CSList * cslist, ContiguousSet * cset, long bounds[2]){
    if(cset == NULL){
        // do nothing
    }
    else if(cset == cslist->cset){
        // do nothing
    }
    else if(cslist->next == NULL){
        cslist->next = init_empty_CSList();
        cslist       = cslist->next;
        cslist->cset = cset;

        long lo = LONG_MIN;
        long hi = LONG_MAX;
        Block * blk = cset->ends[0];
        // blk should never be NULL
        assert(blk != NULL);
        // if ContiguousSet is valid, ends[0] should have no prev
        assert(blk->cnr[0] == NULL);
        for(; blk != NULL; blk = blk->cnr[1]){
            if(blk->pos[0] <= bounds[0] && blk->pos[0] >= lo){
                cslist->bound[LO] = blk;
                lo = blk->pos[0];
            }
            if(blk->pos[1] >= bounds[1] && blk->pos[1] <= hi){
                cslist->bound[HI] = blk;
                hi = blk->pos[1];
            }
        }
    }
    else{
        add_cset_CSList(cslist->next, cset, bounds);
    }
}

void add_blk_CSList(CSList * cslist, Block * blk){
  // first entry of empty CSList
  if(blk == NULL){
    fprintf(stderr, "I got a null block\n");
    exit(EXIT_FAILURE);
  }
  else if(cslist->bound[LO] == NULL && cslist->bound[HI] == NULL){
    cslist->bound[LO] = blk;
    cslist->bound[HI] = blk;
    cslist->cset = blk->cset;
  }
  else if(cslist->cset == blk->cset){
    if(cslist->bound[HI] == NULL || blk->pos[LO] > cslist->bound[HI]->pos[HI]){
        cslist->bound[HI] = blk;
    }
    if(cslist->bound[LO] == NULL || blk->pos[HI] < cslist->bound[LO]->pos[LO]){
        cslist->bound[LO] = blk;
    }
  }
  else if(cslist->next == NULL){
    cslist->next = init_CSList(blk);
  }
  else{
    add_blk_CSList(cslist->next, blk);
  }
}

void free_CSList(CSList * cslist){
  if(cslist->next != NULL)
    free_CSList(cslist->next);
  free(cslist);
}
