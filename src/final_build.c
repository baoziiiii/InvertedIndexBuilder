#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include "termid.h"
#include "lexicon.h"
#include "hashmap.h"
#include "model.h"
#include "var_bytes.h"


// build lexicon and inverted list from merged file
void build(){
    int L = 0;
    char** _term_id_map = read_term_id_map(&L);
    
    FILE* f_in = fopen("tmp/merged", "r");
    FILE* f_lex = fopen("output/lexicon","w+");
    FILE* f_out = fopen("output/inverted_list", "w+");

    term_entry* te = (term_entry*) malloc(sizeof(term_entry));
    te->chunk_list_head = NULL;

    file_buffer* fb_i = init_dynamic_buffer(INPUT_BUFFER);
    fb_i -> f = f_in;
    
    file_buffer* fb_o = init_dynamic_buffer(OUTPUT_BUFFER);
    fb_o -> f = f_out;

    printf("Building final files...\n");

    while(next_record_from_file_buffer(fb_i, te) == true){
        lexicon* lex = (lexicon*) malloc(sizeof(lexicon));
        lex->term = _term_id_map[te->term_id];
        lex->term_length = strlen(lex->term);
        
        lex->info_start = write_to_final_inverted_list(fb_o, te, false);
        // lex->info_length = te->total_size - sizeof(te->term_id) - sizeof(te->total_size);

        write_lexicon_file(f_lex, lex);
        _free_te(te);
        free(lex);
        te =  (term_entry*) malloc(sizeof(term_entry));
        te -> chunk_list_head = NULL;
    }

    write_to_final_inverted_list(fb_o, NULL, true);
    fclose(f_out);

    fclose(f_in);
    fclose(f_lex);

    free_file_buffer(fb_i);
    free_file_buffer(fb_o);
    _free_te(te);
    free_term_id_map(_term_id_map, L);
}