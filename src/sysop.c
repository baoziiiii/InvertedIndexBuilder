#include <stdio.h>

int remove_files(char* prefix){
   char filename[1024];
   int i = 0;
   while( i < 10000 ){
      snprintf(filename, sizeof(filename), "%s-%d", prefix, i);
      FILE* f = fopen(filename,"r");
      if( f == NULL ){
            i++;
            continue;
      }
      fclose(f);
      remove(filename);
      i ++;
   }
   return -1;
}