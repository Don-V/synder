#include "string.h"

#include "global.h"
#include "genome.h"
#include "contig.h"

/** Allocate memory for Genome *name* of size *size*.
 *
 * @param name genome name (e.g. "Arabidopsis_thaliana")
 * @param size number of child Contig structs (e.g. chromosomes or scaffolds) 
 *
 * @return pointer to new Genome struct 
 * */
Genome * init_genome(char * name, size_t size){
    Genome * gen = (Genome*)malloc(sizeof(Genome));
    gen->name = strdup(name);
    gen->size = size;
    gen->contig = (Contig**)malloc(size * sizeof(Contig*));
    return(gen);
}

/**
 * Recursively tree all memory
 *
 * For each Contig in the contig field, calls free_contig.
 *
 * @param pointer to a Genome struct
 */
void free_genome(Genome * genome){
    if(genome){
        for(int i = 0; i < genome->size; i++){
            free_contig(genome->contig[i]);
        }
        free(genome->contig);
        free(genome->name);
        free(genome);
    }
}

/** Recursively print a genome. */
void print_genome(Genome * genome){
    printf(">\t%s\t%lu\n", genome->name, genome->size);
    for(int i = 0; i < genome->size; i++){
        print_contig(genome->contig[i]);
    }
}
