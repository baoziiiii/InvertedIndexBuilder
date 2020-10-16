#ifndef TERM_ID_H
#define TERM_ID_H

#include <stdio.h>
#include "model.h"
#include "hashmap.h"

/*  global term<-->term_id map. Used in sort, merge and final build phases */
extern char** term_id_map;

void init_term_id_map();
void flush_term_to_file(bool end, int total);
void write_term_id_map(int term_id, char* term, int len, int total);
char** read_term_id_map(int* retlen);
void free_term_id_map(char** tm, int len);

#endif
