#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "ui.h"
#include "io.h"
#include "synmap.h"
#include "analysis.h"
#include "test.h"
#include "contiguous.h"
#include "lev.h"

int main(int argc, char *argv[])
{

  Synmap *syn = NULL;

  FILE * db_file = fopen("db/a_b.txt", "r");
  FILE * query_file = fopen("sample-inputs/at.gff", "r");
  FILE * hit_file = fopen("sample-inputs/hits.syn", "r");

  syn = load_synmap(db_file, 0);

  printf("contiguous_query\n");

  contiguous_query(syn, query_file, false);

  printf("\n\n");

  printf("analysis_filter\n");
  int width = 5000;
  analysis_filter(syn, hit_file, single_advocate, &width);

  printf("\n\n");

  rewind(query_file);
  printf("analysis_count\n");
  analysis_count(syn, query_file);

  fclose(db_file);
  fclose(query_file);
  fclose(hit_file);

  if (syn)
    free_synmap(syn);

  return (EXIT_SUCCESS);
}
