#define _GNU_SOURCE
//#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include<argp.h>

#include "ui.h"

#define MAX_POS 3




/** contains all legal arguments from all subcommands */
Arguments create_Arguments() {
    Arguments args = {
        .synfile = NULL,
        .intfile = NULL,
        .hitfile = NULL,
        .db_filename = NULL,
        .cmd = NULL,
        .pos = (char **)malloc(MAX_POS * sizeof(char *)),
        .test = false,
	    .arg_max = 0,
		.db_status = 0
    };
    memset(args.pos, 0, MAX_POS * sizeof(char *));
    return args;
}

void close_Arguments(Arguments arg){
    if(arg.synfile)
        fclose(arg.synfile);
    if(arg.intfile)
        fclose(arg.intfile);
    if(arg.hitfile)
        fclose(arg.hitfile);
    if(arg.db_filename)
        free(arg.db_filename);
    if((arg.pos)){
        for(int i = 0; i < 3; i++){
            if(arg.pos[i])
                free(arg.pos[i]);
        }
        free(arg.pos);}
    if(arg.cmd){
        free(arg.cmd);}
}

void print_args(Arguments args){
    printf("stub\n");
}

void check_file(FILE * fp, char * name){
    if(fp == NULL){
        fprintf(stderr, "ERROR: Failed to open '%s'\n", name);
        exit(EXIT_FAILURE);
    }
}

// ARGP stuff here

const char *argp_program_version=
  "synfull 0.1.0";

const char *argp_program_bug_address =
  "https://github.com/arendsee/cf";

static char doc[] =
  "Synful -- Please Sir, I want some more... Synteny.";

static char args_doc[]= "-d SYNTENY_FILE QUERY_TARGET DEST_DIR \n -i GFF_FILE -s SYNTENY_DATABASE";

static struct argp_option options[]={
  {"gendb", 'd', "SYNTENY_FILE QUERY TARGET DEST_DIR", 0, "Generate sytneny database from file",0},
  {"input", 'i', "GFF_FILE", 0, "Target gff",1},
  {"database", 's', "SYNTENY_DATABASE", 0,"Synteny database generated by -d",1},
  {"command",'c',"COMMAND",0, "Run synful using the provided command",1},
  {0,0,0,0, "The following arguments are used in conjunction with -c",2},
  {0,0,0,0, "map - print target intervals overlapping each query interval",2},
  {0,0,0,0, "count - like map but prints only the number that overlap",2},
  {0,0,0,0, "filter - print query-to-target links consistent with the synteny map",2},
  {0,0,0,0, "pred - predict target search spaces for each query interval",2},
  {0}
};


/* Parse a single option. */
static int
parse_opt (int key, char *arg, struct argp_state *state)
{
  /* Get the input argument from argp_parse, which we
     know is a pointer to our arguments structure. */
  struct Arguments *arguments = state->input;
  int db_state = arguments->db_status;
  int arg_pos;
  FILE * temp;
  switch (key)
    {
    case 'd':
	  if(db_state == 2) argp_failure(state,1,0,"invalid argument");
	  arguments->db_status = 1;
      arguments->arg_max = 3;
      temp = fopen(arg, "r");
      check_file(temp, arg);
	  fclose(temp);
	  arguments->db_filename = strdup(arg);
	  break;
    case 's':
	  if(db_state == 1) argp_failure(state,1,0,"invalid argument");
	  arguments->db_status =2;
      arguments->synfile = fopen(arg,"r");
      check_file(arguments->synfile, arg);
      break;
    case 'i':
	  if(db_state == 1) argp_failure(state,1,0,"invalid argument");
	  arguments->db_status =2;
      arguments->intfile = fopen(arg,"r");
      check_file(arguments->intfile, arg);
      break;
    case 'f':
	  if(db_state == 1) argp_failure(state,1,0,"invalid argument");
	  arguments->db_status =2;
      arguments->hitfile = fopen(arg,"r");
      check_file(arguments->hitfile, arg);
      break;
	case 'c': 
	  if(db_state == 1) argp_failure(state,1,0,"invalid argument");
	  arguments->db_status =2;
      arguments->cmd = strdup(arg);
      break;
	
	case ARGP_KEY_ARG:
      	arg_pos =  MAX_POS - arguments->arg_max;
     	arguments->arg_max --;
	  	if(arguments->arg_max >=0)
      		arguments->pos[arg_pos] = arg;
	  break;

    case ARGP_KEY_END:
	  if(arguments->arg_max > 0){
		argp_failure(state,1,0,"Too few arguements");
	  } else if (arguments->arg_max < 0){
        argp_failure(state,1,0,"Too many arguments");
	  }
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}


static struct argp argp = {options, parse_opt, args_doc, doc};

Arguments parse_command(int argc, char * argv[]){
    Arguments args =create_Arguments();
    if(argc ==2 &&strcmp(argv[1],"test")==0){
		args.test = true;
	 	return args;
    }

	argp_parse(&argp, argc, argv, 0, 0, &args);

	return args;
}
