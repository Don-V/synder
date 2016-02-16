#define _GNU_SOURCE
#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "ui.h"

/** contains all legal arguments from all subcommands */
Arguments create_Arguments() {
    Arguments args = {
        .synfile = NULL,
        .intfile = NULL,
        .db_filename  = NULL,
        .cmd = NULL
    };
    return args;
}

void close_Arguments(Arguments arg){
    if(arg.synfile)
        fclose(arg.synfile);
    if(arg.intfile)
        fclose(arg.intfile);
    if(arg.db_filename)
        free(arg.db_filename);
    if(arg.cmd)
        free(arg.cmd);
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

void print_help(){
    printf("USAGE: synfull -d SYNTENY_FILE QUERY TARGET DEST_DIR\n");
    printf("USAGE: synfull -i GFF_FILE -s SYNTENY_DB\n");
    printf("$ synfull -d at-al.tab athalian alyrata db\n");
    printf("$ synfull -i at.gff -s db/at_al.tab -c count\n");
    exit(EXIT_SUCCESS);
}

Arguments parse_command(int argc, char * argv[]){
    if(argc == 1)
        print_help();
    int opt;
    FILE * temp;
    Arguments args = create_Arguments();
    while((opt = getopt(argc, argv, "hd:s:i:c:")) != -1){
        switch(opt) {
            case 'h':
                print_help();
                break;
            case 'd':
                temp = fopen(optarg, "r");
                check_file(temp, optarg);
                fclose(temp);
                args.db_filename = strdup(optarg);
                break;
            case 's':
                args.synfile = fopen(optarg, "r");
                check_file(args.synfile, optarg);
                break;
            case 'i':
                args.intfile = fopen(optarg, "r");
                check_file(args.intfile, optarg);
                break;
            case 'c':
                args.cmd = strdup(optarg);
                break;
            case '?':
                exit(EXIT_FAILURE);
        }
    }
    return args;
}
