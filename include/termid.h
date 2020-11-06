#ifndef TERM_ID_H
#define TERM_ID_H

#include <stdio.h>
#include "model.h"
#include "hashmap.h"

/*  global term<-->term_id map(./tmp/tmp_term) Generated in parse phase, used in sort, merge phases for intermediate process.*/
extern char** term_id_map;

void init_term_id_map();
void flush_term_to_file(bool end, int total);
void write_term_id_map(int term_id, char* term, int len, int total);
char** read_term_id_map(int* retlen);
void free_term_id_map(char** tm, int len);

#endif
