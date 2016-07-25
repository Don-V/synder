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

  syn = load_synmap(db_file, 0);

  FILE * query_file = fopen("sample-inputs/at.gff", "r");

  contiguous_query(syn, query_file, false);

  if (syn)
    free_synmap(syn);

  return (EXIT_SUCCESS);
}
