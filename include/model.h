#ifndef MODEL_H
#define MODEL_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>


/*  doc model */
typedef struct DE{
    int doc_id; 
    int size_of_doc;
    char* URL;
    long offset;
}doc_entry;

/* See lexicon.h for lexicon model */

/*  posting model  */
//  TE(term_id) -> TC(doc_id)

// Term Entry Chunk, by doc_id
typedef struct TC{
    int doc_id; // doc id
    int freq;   // freq(length of PN list)
    struct TC* next; // next TC Node
}term_chunk;

// Term Entry
typedef struct TE{
    int total_size; // total size of this TE node = sizeof(term_id) + sizeof(total_sizes) + sizeof(tc_length) + sum(full size of each TC node) 
    int term_id;   // term id
    int tc_length; // TC list length
    struct TC* chunk_list_head; // TC list head
    struct TC* chunk_list_tail; // TC list tail
}term_entry;


typedef struct iv{
    FILE* f;
    int length;
    long block_ldoc_table_offset;
    long block_size_table_offset;
    long block_offset; 
    term_chunk* block_cache;
    term_chunk* block_cache_ptr;
    int block_curr_ldoc;
    int block_curr_sdoc;
    int block_curr_cnt;
    int curr_doc;
    int curr_freq;
    int maxdocID;
}IV;


/*  Dynamic Buffer  */
typedef struct{
    FILE* f;
    char* buffer;
    int size;
    int curr;
    int max;
}file_buffer;

// global general buffer setting
#define INPUT_BUFFER 100000000
#define OUTPUT_BUFFER 100000000

#define IS_LOWER(x)     ((x) <= 'z' && (x) >= 'a')  
#define IS_UPPER(x)     ((x) <= 'Z' && (x) >= 'A')
#define IS_ALPHA(x)     (IS_LOWER(x) || IS_UPPER(x))
#define IS_ALPHANUM(x)  (IS_ALPHA(x) || (x) >= '0' && (x) <= '9' )

/* Implemented in model_support.c */

/*  model operations  */
bool merge_same_term(term_entry* dst, term_entry*src); 
void print_term_entry(doc_entry** doc_table ,term_entry* te);
bool _free_te(term_entry* te);

/*  model output  */
bool write_doc_table(file_buffer* fb, doc_entry* de, bool flush);
bool next_doc(file_buffer* fb,  doc_entry* de);
void write_doc_table_end(file_buffer* fb, int total);

/*  inverted list op  */
#define BLOCK_SIZE 128
term_chunk* read_block_to_cache(IV* lp);
long write_to_final_inverted_list(file_buffer* fb, term_entry*te, bool flush);


/*  Operate the term model by dynamic buffer  */
file_buffer* init_dynamic_buffer(int size);
void free_file_buffer(file_buffer* fb);
bool next_record_from_file_buffer(file_buffer* fb, term_entry* te);
bool write_record(file_buffer* fb, term_entry* te, bool flush);
void flush_buffer_to_file(file_buffer* fb);


#endif
