#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "map.h"
#include "contig.h"
#include "synmap.h"

// A local utility structure used filter and store contiguous sets
typedef struct CSList{
  struct CSList * next;
  Block * bound[2]; 
  int setid;
} CSList;
CSList * init_empty_CSList();
CSList * init_CSList(Block * blk);
void add_blk_CSList(CSList * cslist, Block * blk);
void free_CSList(CSList * cslist);

typedef struct SI_Bound{
  uint bound;
  int mask; 
} SI_Bound;

SI_Bound * get_si_bound(
  uint q,
  uint set_bounds[2],
  Block * blk_bounds[2],
  Direction d,
  bool inverted
);

int get_flag(SI_Bound * br[2]);

void find_search_intervals(Synmap * syn, FILE * intfile, bool pblock)
{

  // start and stop positions read from input line
  uint bounds[2];
  // Max and min nodes in current contiguous set
  uint set_bounds[2]; 
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
  int chrid;
  // Row output of itree
  ResultContig * rc;

  char *line = (char *) malloc(LINE_BUFFER_SIZE * sizeof(char));
  while (fgets(line, LINE_BUFFER_SIZE, intfile) && !feof(intfile)) {
  if (!sscanf
    (line, "%d %*s %*s %d %d %*s %*c %*s %s\n", &chrid, &bounds[LO], &bounds[HI],
     seqname)) {
    printf("invalid input\n");
    continue;
  }

    rc = get_region(SGC(syn, 0, chrid), bounds[LO], bounds[HI]);
 
    cslist = init_empty_CSList();
    root = cslist;
 
    // get list of highest and lowest members of each contiguous set
    for(int i = 0; i < rc->contig->size; i++){
      add_blk_CSList(cslist, rc->contig->block[i]); 
    }
 
    // Iterate through each contiguous set, for each find search interval(s)
    for(; cslist != NULL; cslist = cslist->next){
 
      set_bounds[LO] = get_set_bound(cslist->bound[LO], LO);
      set_bounds[HI] = get_set_bound(cslist->bound[HI], HI);
 
      blk_bounds[LO] = cslist->bound[LO];
      blk_bounds[HI] = cslist->bound[HI];
 
      inverted = blk_bounds[HI]->over->strand == '-';
 
      bound_results[inverted ^ LO] =
        get_si_bound(bounds[LO], set_bounds, blk_bounds, LO, inverted);
      bound_results[inverted ^ HI] =
        get_si_bound(bounds[HI], set_bounds, blk_bounds, HI, inverted);
 
      printf("%s\t%s\t%i\t%i\t%s\t%i\t%i\t.\t%i\n",
        seqname,
        blk_bounds[LO]->parent->name,
        bounds[LO], bounds[HI],
        blk_bounds[LO]->over->parent->name,
        bound_results[LO]->bound, bound_results[HI]->bound,
        get_flag(bound_results));
 
    }
 
    free_partial_ResultContig(rc);
    free_CSList(root);
  }

  free(line);
}

int get_flag(SI_Bound * br[2]){
    // The HI and LO bounds each have a 4 bit mask describing the overlap
    // state. Since the states are encoded in the odd bits, shifting one
    // mask one bit over and ORing them together, yields a single 8 bit
    // mask fully describing the joint state.
    int mask = ( br[LO]->mask << 1 ) | br[HI]->mask;

    // I determine the overlap cases based on this 8 bit mask, Currently I
    // don't use the other bits in the int for anything, but just to be
    // safe, I'll AND them out.
    //
    // A - anchored
    // B - bound
    // U - unbound
    // X - extreme
    // TODO: decide on a stable set of flags to return to the user, for now, setting all to -1
    switch(mask & 0xFF){
      case F_AA:
        return -1;
      case F_AB:
        return -1;
      case F_BA:
        return -1;
      case F_BB:
        return -1;
      case F_UA:
        return -1;
      case F_XA:
        return -1;
      case F_AU:
        return -1;
      case F_UU:
        return -1;
      case F_UB:
        return -1;
      case F_BU:
        return -1;
      case F_AX:
        return -1;
      case F_UX:
        return -1;
      case F_BX:
        return -1;
      case F_XB:
        return -1;
      case F_XU:
        return -1;
      case F_XX:
        return -1;
      default:
        return -1;
    }
}

SI_Bound * init_SI_Bound(uint bound, int mask){
  SI_Bound * br = (SI_Bound *)malloc(sizeof(SI_Bound));
  br->bound = bound;
  br->mask = mask;
  return br;
}

SI_Bound * get_si_bound(
  uint q,
  uint set_bounds[2],
  Block * blk_bounds[2],
  Direction d,
  bool inverted)
{
  // Invert orientation mapping to target if search interval is inverted
  Direction vd = inverted ? !d : d;
  // See contiguous.h
  int mask = 0;
  // non-zero to ease debugging
  uint bound = 444444;
  // +1 or -1 values for snapping above or below a point
  int offset = d ? 1 : -1;
  // Invert these values if the search interval is inverted
  offset = inverted ? -1 * offset : offset;

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
  uint pnt_a = blk_bounds[!d]->pos[!d];
  uint pnt_b = blk_bounds[!d]->pos[ d];
  uint pnt_c = blk_bounds[ d]->pos[!d];
  uint pnt_d = blk_bounds[ d]->pos[ d];


  // This may occur when there is only one element in the ContiguousSet
  //          |x-----y|
  //       ...a=======b      
  //         ^
  //   <---q
  // q < x
  if(REL_LT(q, set_bounds[!d], d)){
    bound = blk_bounds[!d]->over->pos[!vd] - offset;
    mask |= UNBOUND;
  }

  //   |x---[===]-------a=======b...y|   ...    F===
  //                   ^
  //              --q
  // q < a
  else if(REL_LT(q, pnt_a, d)){
    bound = blk_bounds[!d]->over->pos[!vd] - offset;
    mask |= BOUND;
  }

  //   |x...a=======b...y|   ...    F===
  //                ^
  //        <---q
  // q < b
  else if(REL_LE(q, pnt_b, d)){
    bound = blk_bounds[!d]->over->pos[vd];
    mask |= ANCHORED;
  }

  //   |x...a=======b-------c=======d...y|  ...  F===
  //                       ^
  //                  <--q
  // q < c && q > b
  //   (q > b test required since blk_bounds[LO] can equal blk_bounds[HI])
  else if(REL_LT(q, pnt_c, d) && REL_GT(q, pnt_b, d))
  {
    bound = blk_bounds[d]->over->pos[!vd] - offset;
    mask |= BOUND;
  }

  //   |x...a=======b-------c=======d...y|  ...  F===
  //                                ^
  //             <--------------q
  // q < d
  else if(REL_LE(q, pnt_d, d)){
    bound = blk_bounds[d]->over->pos[vd];
    mask |= ANCHORED;
  }

  //   |x...a=======b-------c=======d-------[===]...y|  ...  F===
  //                                       ^
  //              <----------------------q
  // q < y, (which implies there is a node after d)
  else if(REL_LE(q, set_bounds[d], d)){
    bound = blk_bounds[d]->cnr[d]->over->pos[!vd] - offset;
    mask |= BOUND;
  }

  // If none of the above, the bound beyond anything in the contiguous set
  // In this case, the hi and lo Contiguous nodes will be the same
  else {
    // Get nearest non-overlapping sequence
    Block * downstream_blk = blk_bounds[d]->over->adj[vd];
 
    // adjacent block on TARGET side exists
    if(downstream_blk != NULL){

      //    |x...--a=======b|
      //    |x...--c=======d|  ...  F===
      //                           ^
      //                  <-----------q
      // q >= F  ***ON QUERY SIDE***
      if(!inverted && REL_GT(q, downstream_blk->over->pos[!d], d)){
        // TODO: This is a weird case, how should I handle it?
        mask |= ANCHORED;
        bound = downstream_blk->pos[!d] - offset;
      }

      //    |x...--a=======b|
      //    |x...--c=======d|  ...  F===
      //                           ^
      //                    <---q
      else {
        mask |= UNBOUND;
        bound = downstream_blk->pos[!vd] - offset;
      }
    }
    //    |x...--a=======b|
    //    |x...--c=======d|  ...  THE_END
    //                    <---q
    // query is further out than ANYTHING in the synteny map
    else {
      mask |= EXTREME;
      // TODO: Is there a better way to handle unbound extremes?
      bound = d ? 999999999 : 0;
    }
  }

  return init_SI_Bound(bound, mask);
}


CSList * init_empty_CSList(){
  CSList * cslist = (CSList *)malloc(sizeof(CSList));
  cslist->next = NULL;
  cslist->bound[LO] = NULL;
  cslist->bound[HI] = NULL;
  return(cslist);
}

CSList * init_CSList(Block * blk){
  CSList * cslist = (CSList *)malloc(sizeof(CSList));
  cslist->next = NULL;
  cslist->bound[LO] = blk;
  cslist->bound[HI] = blk;
  cslist->setid = blk->setid;
  return(cslist);
}

void add_blk_CSList(CSList * cslist, Block * blk){
  // first entry of empty CSList
  if(cslist->bound[LO] == NULL && cslist->bound[HI] == NULL){
    cslist->bound[LO] = blk;
    cslist->bound[HI] = blk;
    cslist->setid = blk->setid;
  }
  else if(cslist->setid == blk->setid){
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
