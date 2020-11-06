
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "model.h"
#include "merge_phase.h"
#include "termid.h"


#define INTERMEDIATE_SIZE 50000000


int Comparator(const void * e1, const void * e2){ return strcmp(term_id_map[(*(term_entry**)e1)->term_id],term_id_map[(*(term_entry**)e2)->term_id]);} 

int read_intermediate(char** term_id_map, term_entry* buffer[], int max, char* filename){
    FILE* f = fopen(filename, "r");
    if(f == NULL){
        printf("Can not open %s\n", filename);
        exit(EXIT_FAILURE);
    }

    file_buffer* fb_i = init_dynamic_buffer(INPUT_BUFFER);
    fb_i -> f = f;

    int readed = 0;
    while(readed < max){
        term_entry* te = (term_entry*)malloc(sizeof(term_entry));
        te->chunk_list_head = NULL;
        if(next_record_from_file_buffer(fb_i,te) == true){
            buffer[readed] = te;
            readed ++;
        }else{
            _free_te(te);
            break;
        }
    }
    free_file_buffer(fb_i);
    fclose(f);
    return readed;
}

  
term_entry* sort_buffer[INTERMEDIATE_SIZE];

int sort(int num_intermediate_files){

    char filename[1024];
    
    for(int i = 0; i < num_intermediate_files; i++){
        snprintf(filename, sizeof(filename), "%s-%d", "tmp/sorted", i);
        FILE* f = fopen(filename, "w+");
        if(f == NULL){
            printf("Can not create %s\n", filename);
            exit(EXIT_FAILURE);
        }
        snprintf(filename, sizeof(filename), "%s-%d", "tmp/intermediate", i);
        int readed = read_intermediate(term_id_map, sort_buffer, INTERMEDIATE_SIZE, filename);
        qsort(sort_buffer, readed, sizeof(term_entry*), Comparator);
    
        file_buffer* fb = init_dynamic_buffer(OUTPUT_BUFFER);
        fb -> f = f;
        for(int j = 0; j < readed; j++)
            write_record(fb, sort_buffer[j],false);
        write_record(fb,NULL,true);
        for(int j = 0; j < readed; j++)
            _free_te(sort_buffer[j]);
        free_file_buffer(fb);
        fclose(f);
        remove(filename);
    }
    return num_intermediate_files;
}
