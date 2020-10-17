#ifndef LEXICON_H
#define LEXICON_H

#include <stdio.h>
#include "hashmap.h"

// word length limit
#define WORD_LENGTH_MIN 1
#define WORD_LENGTH_MAX 15

/* Lexicon model */
typedef struct{
    int term_length;
    char* term;
    long info_start;
    // int info_length;
}lexicon;

void write_lexicon_file(FILE* f,lexicon* lex);
bool read_lexicon_file();
bool query(char* s);
void close_lexicon_file();

#endif