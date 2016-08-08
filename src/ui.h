#ifndef __UI_U__
#define __UI_U__

#include <stdio.h>
#include <stdbool.h>

#include "global.h"

/** Parsed command line arguments */
typedef struct Arguments {
  FILE *synfile;
  FILE *intfile;
  FILE *hitfile;
  char *db_filename;
  char *cmd;
  char **pos;
  bool swap;
} Arguments;

void close_Arguments(Arguments arg);

Arguments create_Arguments();

void print_version();

void print_help();

void print_args(Arguments args);

Arguments parse_command(int argc, char *argv[]);

#endif
