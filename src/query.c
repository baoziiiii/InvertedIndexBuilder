#include <stdio.h>
#include <string.h>
#include "lexicon.h"
#include "model.h"


/*  Hash Map  */
// read from lexicon to a map in memory , used for query
struct hashmap * query_map;

typedef struct
{
   char key_string[WORD_LENGTH_MAX+1];
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

doc_entry** doc_table; // doc_entry array
int doc_table_L = 0;

bool read_doc_file(struct hashmap* query_map){
    FILE* f_doc = fopen("output/doc_table","r");
    file_buffer* fb = init_dynamic_buffer(100000000);
    fb -> f = f_doc;

    if(f_doc == NULL)
        return false;
    int L = 0;
    fread(&L, sizeof(int), 1, f_doc);

    doc_table = (doc_entry **)malloc(L*sizeof(doc_entry*));
    for(int i = 0; i < L; i++){
        doc_entry* de = (doc_entry*)malloc(sizeof(doc_entry));
        next_doc(fb,de);
        doc_table[de->doc_id] = de;
    }
    doc_table_L = L;
    free_file_buffer(fb);
    fclose(f_doc);
    return true;
}

void free_doc_table(){
    for(int i = 0; i < doc_table_L; i++){
        free(doc_table[i]->URL);
        free(doc_table[i]);
    }
    free(doc_table);
}

bool init_query_database(){
    query_map = hashmap_new(sizeof(lexicon_el), 0, 0, 0, 
                                l_hash, l_compare, NULL);
    
    if(read_lexicon_file(query_map) == false || read_doc_file(query_map) == false)
        return false;
    return true;
}


bool _free_lex(const void * value, void * x){
   free((lexicon_el*)value);
   return true;
}

void close_query_database(){
    free_doc_table();
    hashmap_scan(query_map, &_free_lex, NULL);
    hashmap_free(query_map);
}


// query the lexicon map for offset in inverted index
bool query(char* s){
    FILE* f_inv = fopen("output/inverted_list","r");

    lexicon_el* key =(lexicon_el*) malloc(sizeof(lexicon_el));
        
    lexicon_el* value;

    memset(key->key_string, 0, WORD_LENGTH_MAX);
    memcpy(key->key_string, s, strlen(s));
    key->key_string[WORD_LENGTH_MAX - 1] = 0;
    value = hashmap_get(query_map,key);
    if(value == NULL)
        return false;

    term_entry* te = (term_entry*) malloc(sizeof(term_entry));

    read_inverted_list(f_inv, value->offset, 0, te);
    print_term_entry(doc_table, te);
    _free_te(te);
    free(key);
    fclose(f_inv);
    return true;
}