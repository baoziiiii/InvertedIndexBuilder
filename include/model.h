#ifndef MODEL_H
#define MODEL_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/*  doc model */
typedef struct DE{
    int doc_id; //
    int length;
    char* URL;
}doc_entry;

/* See lexicon.h for lexicon model */

/*  posting model  */
//  TE(term_id) -> TC(doc_id) -> PN(position)

// Position Node 
typedef struct PN{
    int position; // term position in the doc
    struct PN* next; // next position node
}position_node;

// Term Entry Chunk, by doc_id
typedef struct TC{
    int doc_id; // doc id
    int freq;   // freq(length of PN list)
    position_node* head; // PN list head
    position_node* tail;
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


/*  Dynamic Buffer  */
typedef struct{
    FILE* f;
    char* buffer;
    int size;
    int curr;
    int max;
}file_buffer;

#define INPUT_BUFFER 100000000
#define OUTPUT_BUFFER 100000000

/*  model operations  */
bool merge_same_term(term_entry* dst, term_entry*src);
void print_term_entry(term_entry* te);
bool _free_te(term_entry* te);

/*  model output  */
void write_doc_table(doc_entry *de);
void read_inverted_list(FILE* f, int offset, int length, term_entry* te);
int write_to_final_inverted_list(file_buffer* fb, term_entry*te, bool flush);

/*  Operate the term model by dynamic buffer  */
file_buffer* init_dynamic_buffer(int size);
void free_file_buffer(file_buffer* fb);
bool next_record_from_file_buffer(file_buffer* fb, term_entry* te);
bool write_record(file_buffer* fb, term_entry* te, bool flush);
void flush_buffer_to_file(file_buffer* fb);



#endif
