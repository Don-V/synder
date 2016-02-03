#define _GNU_SOURCE
#include <getopt.h>

#include "ui.h"
#include "ftypes.h"

Arguments create_Arguments() {
    Arguments args = {
        .a = false,
        .b = false,
        .c = false
    };
    return args;
}

void close_Arguments(Arguments arg){
    if(arg.synfile)
        fclose(arg.synfile);
}

void print_args(Arguments args){
    printf("a=%d b=%d c=%d\n",
            args.a ? 1 : 0,
            args.b ? 1 : 0,
            args.c ? 1 : 0);
}

Arguments parse_command(int argc, char * argv[]){
    int opt;
    Arguments args = create_Arguments();
    while((opt = getopt(argc, argv, "abcf:")) != -1){
        switch(opt) {
            case 'a':
                args.a = true;
                break;
            case 'b': 
                args.b = true;
                break;
            case 'c': 
                args.c = true;
                break;
            case 'f':
                args.synfile = fopen(optarg, "r");
                if(args.synfile == NULL){
                    fprintf(stderr, "ERROR: Failed to open synteny file '%s'\n", optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            case '?':
                exit(EXIT_FAILURE);
        }
    }
    for(; optind < argc; optind++){
        printf("Positional: %s\n", argv[optind]);
    }
    return args;
}
