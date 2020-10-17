#ifndef QUERY_H
#define QUERY_H
#include "lexicon.h"

typedef struct
{
   char key_string[WORD_LENGTH_MAX+1];
   int offset;
} lexicon_el;

bool init_query_database();
bool query(char* s);
void close_query_database();

#endif