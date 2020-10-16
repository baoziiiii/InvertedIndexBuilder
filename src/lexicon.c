/*  Hash Map  */

#include <stdio.h>
#include <string.h>
#include "lexicon.h"
#include "model.h"
#include "hashmap.h"

#define DYNAMIC_BUFFER 10000000
#define MAX_KEY_LENGTH 13


/*  Hash Map  */
// read from lexicon to a map in memory , used for query
struct hashmap * query_map;

typedef struct
{
   char key_string[MAX_KEY_LENGTH+1];
   int offset;
} lexicon_el;

uint64_t l_hash(const void *item, uint64_t seed0, uint64_t seed1) {
    const lexicon_el* data = item;
    return hashmap_sip(data->key_string, strlen(data->key_string), seed0, seed1);
}

int l_compare(const void *a, const void *b, void *udata) {
    const lexicon_el *ua = a;
    const lexicon_el *ub = b;
    return strcmp(ua->key_string, ub->key_string);
}

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
    int total_size = term_length + 3*sizeof(int);
    if(fb->curr + total_size > DYNAMIC_BUFFER){
        fseek(fb->f,-(DYNAMIC_BUFFER - fb->curr),SEEK_CUR);
        read_batch(fb);
    }
    fb->curr += sizeof(int);

    memcpy(ll->key_string, dynamic_buffer + fb->curr, term_length);
    fb->curr += term_length;

    memcpy(&ll->offset, dynamic_buffer + fb->curr,sizeof(int));
    fb->curr += 2*sizeof(int);
    return true;
}


bool read_lexicon_file(){
    FILE* f_lex = fopen("output/lexicon","r");
    file_buffer* fb = init_dynamic_buffer(DYNAMIC_BUFFER);
    fb -> f = f_lex;

    if(f_lex == NULL)
        return false;

    query_map = hashmap_new(sizeof(lexicon_el), 0, 0, 0, 
                                l_hash, l_compare, NULL);
    
    lexicon_el* ll =(lexicon_el*) malloc(sizeof(lexicon_el));
    memset(ll->key_string,0,MAX_KEY_LENGTH);

    while(read_next_term(fb,ll) == true){
        ll->key_string[MAX_KEY_LENGTH - 1] = 0;
        hashmap_set(query_map, ll);
    }

    free_file_buffer(fb);

    fclose(f_lex);
    return true;
}

void write_lexicon_file(FILE* f_lex, lexicon* lex){
    fwrite(&lex->term_length,sizeof(int),1,f_lex);
    fwrite(lex->term,1,lex->term_length,f_lex);
    fwrite(&lex->info_start,sizeof(int),1,f_lex);
    fwrite(&lex->info_length,sizeof(int),1,f_lex);
}

// query the lexicon map for offset in inverted index
bool query(char* s){
    FILE* f_inv = fopen("output/inverted_list","r");

    lexicon_el* key =(lexicon_el*) malloc(sizeof(lexicon_el));
        
    lexicon_el* value;

    memset(key->key_string, 0, MAX_KEY_LENGTH);
    memcpy(key->key_string, s, strlen(s));
    key->key_string[MAX_KEY_LENGTH - 1] = 0;
    value = hashmap_get(query_map,key);
    if(value == NULL)
        return false;

    term_entry* te = (term_entry*) malloc(sizeof(term_entry));
    read_inverted_list(f_inv, value->offset, 0, te);
    printf("%s\n",s);
    print_term_entry(te);
    _free_te(te);
    free(key);
    fclose(f_inv);
    return true;
}

void close_lexicon_file(){
    hashmap_free(query_map);
}
