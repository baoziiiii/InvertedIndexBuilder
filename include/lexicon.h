#ifndef LEXICON_H
#define LEXICON_H

#include <stdio.h>
#include "hashmap.h"

/* Lexicon model */
typedef struct{
    int term_length;
    char* term;
    int info_start;
    int info_length;
}lexicon;

void write_lexicon_file(FILE* f,lexicon* lex);
bool read_lexicon_file();
bool query(char* s);
void close_lexicon_file();

#endif