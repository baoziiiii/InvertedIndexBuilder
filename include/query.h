#ifndef QUERY_H
#define QUERY_H
#include "lexicon.h"

typedef struct
{
   char key_string[WORD_LENGTH_MAX+1];
   long offset;
} lexicon_el;

#define CONJUNCTIVE_MODE 0
#define DISJUNCTIVE_MODE 1

bool init_query_database();
bool query(char** s, int N, int limit, int MODE, char* doc_path);
void close_query_database();

#endif