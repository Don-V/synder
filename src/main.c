#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui.h"
#include "io.h"
#include "synmap.h"
#include "analysis.h"
#include "test.h"

int main(int argc, char * argv[]){

    Synmap * syn = NULL;

    // ------------------------------------------------------------------------
    // Prep input 
    // ------------------------------------------------------------------------

    Arguments args = parse_command(argc, argv);

    // ------------------------------------------------------------------------
    // Do stuff 
    // ------------------------------------------------------------------------
    
    if(args.test)
        test_all();

    /** \todo Replace a system call to the prepare-data.sh script with a raw
     * synteny file to parser in synmap  */

    // Build database and exit
    if(args.db_filename){
        char cmd[256];
        sprintf(cmd,
                "util/prepare-data.sh -a %s -b %s -i %s -d %s",
                args.pos[0], args.pos[1], args.db_filename, args.pos[2]);
        system(cmd);
        exit(EXIT_SUCCESS);
    }

    if(args.synfile){
        syn = load_synmap(args.synfile);
    }

    if(!(syn || args.test)){
        printf("Nothing to do ...\n");
    }

    if(args.hitfile){
        if(strcmp(args.cmd, "filter") == 0){
            int width = 5000;
            analysis_filter(syn, args.hitfile, single_advocate, &width);
        }
    }

    if(args.intfile){
        if(strcmp(args.cmd, "count") == 0){
            analysis_count(syn, args.intfile);
        }
        else if(strcmp(args.cmd, "map") == 0){
            analysis_map(syn, args.intfile);
        }
        else{
            printf("Command '%s' not recognized\n", args.cmd);
        }
    }


    // ------------------------------------------------------------------------
    // Clean up
    // ------------------------------------------------------------------------
    if(syn)
        free_synmap(syn);
    close_Arguments(args);

    return(EXIT_SUCCESS);
}
