#include <stdio.h>
#include <string.h>
#include <string.h>
#include "model.h"
#include "sort_phase.h"
#include "merge_phase.h"
#include "termid.h"
#include "sysop.h"

int check_number_of_files(char* prefix){
   char filename[1024];
   int i = 0;
   while( i < 10000 ){
      snprintf(filename, sizeof(filename), "%s-%d", prefix, i);
      FILE* f = fopen(filename,"r");
      if( f == NULL )
         return  i == 0 ? -1:i;
      fclose(f);
      i ++;
   }
   return -1;
}

// sort each file and one pass merge
int merge_sort(long memory_limit){
   char s1[] = "tmp/intermediate";
   char s2[] = "tmp/sorted";
   int r = -1;

   int term_id_map_size = 0;
   
   printf("Preparing for sort...\n");

   read_term_id_map(&term_id_map_size);

   r = check_number_of_files(s1);
   if( r == -1 ){
      printf("No file found for: %s\n", s1);
      exit(EXIT_FAILURE);
   }
   
   printf("Sorting...\n");
   sort(r);
   // remove_files(s1);

   printf("Merging sorted files...\n");
   r = check_number_of_files(s2);
   if( r == -1 ){
      printf("No file found for: %s\n", s2);
      exit(EXIT_FAILURE);
   }

   merge(r, r, memory_limit);
   rename("tmp/merged-0","tmp/merged");
   remove_files(s2);

   free_term_id_map(term_id_map, term_id_map_size);
   return 1;
}