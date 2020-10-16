#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> 
#include <string.h>
#include <unistd.h>
#include "termid.h"
#include "hashmap.h"

#define TERM_BUFFER_OUTPUT 50000000

char t_buffer[TERM_BUFFER_OUTPUT];
long cur = 0;
FILE* tmp_term;

char** term_id_map;

void init_term_id_map(){
   tmp_term = fopen("tmp/tmp_term","w+");
   int i = 0;
   fwrite(&i, sizeof(int),1,tmp_term);
}

void flush_term_to_file(bool end, int total){
   if(cur > 0){
      fwrite(t_buffer,1,cur,tmp_term);
   }
   if(end == true){
      fseek(tmp_term, 0, SEEK_SET);
      int L = total;
      fwrite(&L, sizeof(L),1,tmp_term);
      fclose(tmp_term);
   }
   cur = 0;
}

void write_term_id_map(int term_id, char* term, int len, int total){
   int check_len = sizeof(int) + sizeof(int) + len;
   if ( check_len + cur >= TERM_BUFFER_OUTPUT )
        flush_term_to_file(false, total);
   memcpy(t_buffer + cur, &term_id ,sizeof(int));
   cur += sizeof(int);
   memcpy(t_buffer + cur, &len, sizeof(int));
   cur += sizeof(int);
   memcpy(t_buffer + cur, term, len );
   cur += len ;
}

void free_term_id_map(char** tm, int len){
   for(int i = 0; i < len; i++)
      free(tm[i]);
   free(tm);
}

char** read_term_id_map(int* retlen){
   int fd = open("tmp/tmp_term", O_RDONLY);
   int L = -1;
   read(fd, &L, sizeof(int));
   term_id_map = (char **)malloc(L*sizeof(char*));
   for(int i = 0; i < L; i++){
      int id = -1;
      int slen = -1;
      read(fd, &id, sizeof(int));
      read(fd, &slen, sizeof(int));
      term_id_map[id] = (char*)malloc(slen + 1);
      read(fd, term_id_map[id], slen);
      term_id_map[id][slen] = 0;
   }
   for(int i = 0; i < L; i++){
      if(term_id_map[i] == NULL || term_id_map[i][0]== ' ' || term_id_map[i][0] == 0){
         printf("Error 100!");
         exit(0);
      }
   }
   *retlen = L;
   return term_id_map;
}


