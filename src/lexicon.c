#include <stdio.h>
#include <string.h>
#include "lexicon.h"
#include "model.h"
#include "query.h"
#include "hashmap.h"

#define DYNAMIC_BUFFER 10000000


/*  Lexicon IO  */

void read_batch(file_buffer *fb){
    fb->max = fread(fb->buffer, 1, DYNAMIC_BUFFER, fb->f );
    fb->curr = 0;
}

bool read_next_term(file_buffer *fb, lexicon_el* ll){
    if(fb->curr >= fb->max)
        read_batch(fb);
    if(fb->max <= 0)
        return false;

    char* dynamic_buffer = fb->buffer;

    int term_length = 0;
    memcpy(&term_length, dynamic_buffer + fb->curr,sizeof(int));
    int total_size = term_length + sizeof(int) + sizeof(long);
    if(fb->curr + total_size >= DYNAMIC_BUFFER){
        fseek(fb->f,-(DYNAMIC_BUFFER - fb->curr),SEEK_CUR);
        read_batch(fb);
    }
    fb->curr += sizeof(int);

    memcpy(ll->key_string, dynamic_buffer + fb->curr, term_length);
    fb->curr += term_length;

    memcpy(&ll->offset, dynamic_buffer + fb->curr,sizeof(long));
    fb->curr += sizeof(long);
    return true;
}


bool read_lexicon_file(struct hashmap* query_map){
    FILE* f_lex = fopen("output/lexicon","r");
    file_buffer* fb = init_dynamic_buffer(DYNAMIC_BUFFER);
    fb -> f = f_lex;

    if(f_lex == NULL)
        return false;
    
    lexicon_el* ll =(lexicon_el*) malloc(sizeof(lexicon_el));
    memset(ll->key_string,0,WORD_LENGTH_MAX);

    while(read_next_term(fb,ll) == true){
        ll->key_string[WORD_LENGTH_MAX - 1] = 0;
        hashmap_set(query_map, ll);
        ll =(lexicon_el*) malloc(sizeof(lexicon_el));
        memset(ll->key_string,0,WORD_LENGTH_MAX);
    }
    free(ll);
    free_file_buffer(fb);
    fclose(f_lex);
    return true;
}

void write_lexicon_file(FILE* f_lex, lexicon* lex){

    fwrite(&lex->term_length,sizeof(int),1,f_lex);
    fwrite(lex->term,1,lex->term_length,f_lex);
    fwrite(&lex->info_start,sizeof(long),1,f_lex);
    // fwrite(&lex->info_length,sizeof(int),1,f_lex);
}



