#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> 
#include <unistd.h> 
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
// #include <dirent.h>
#include "model.h"
#include "hashmap.h"
#include "merge_sort.h"
#include "termid.h"
#include "lexicon.h"
#include "final_build.h"
#include "query.h"
#include "sysop.h"


// default memory limit 
long MEMORY_LIMIT_SETTING = (long)(1000000000);

#define KEY_MAX_LENGTH (WORD_LENGTH_MAX+1)

// Limit # of intermediate temporary file produced by parsing 
#define MAX_INTERMEDIATE 200


/*  Hash Map  */
/* store mapping between term_id and term during parsing, to ensure same string map to the same id */
struct hashmap *term_map;

typedef struct
{
   char key_string[KEY_MAX_LENGTH];
   int number; // term id
   term_entry* te;
} map_entry;

uint64_t user_hash(const void *item, uint64_t seed0, uint64_t seed1) {
    const map_entry* data = item;
    return hashmap_sip(data->key_string, strlen(data->key_string), seed0, seed1);
}

int user_compare(const void *a, const void *b, void *udata) {
    const map_entry *ua = a;
    const map_entry *ub = b;
    return strcmp(ua->key_string, ub->key_string);
}

/* map-iterator callbacks */

bool handle_map_entry(const void * value, void * fb){
   map_entry* v = (map_entry*)value;
   file_buffer** fbb = (file_buffer**) fb; 
   term_entry* te = v->te; // See model.h
   if( te == NULL )
      return true;
   write_record(*fbb, te ,false); // See IOsupport.c and model.h
   return true;
}

bool _free(const void * value, void * x){
   map_entry* v = (map_entry*)value;
   _free_te(v->te);
   v -> te = NULL;
   return true;
}

/* End of hash map */


/*  Document Parser  */

// memory allocated for hashmap entries 
long memory_allocated = 0; 
file_buffer* doc_buffer;
long doc_id = 0;

void process_term(int len, char* term, int doc_id, int pos);

// parse document 
int process_document(int len, char* buf, long doc_offset){

   doc_entry *de = (doc_entry *)malloc(sizeof(doc_entry));
   de->doc_id = doc_id;

   char* ptr = strchr(buf, '\n');
   int len_url = ptr - buf + 1;
   de->URL = (char *)malloc(len_url);
   memcpy(de->URL, buf, len_url - 1);
   de->URL[len_url - 1] = 0;
   de->size_of_doc = len;
   de->offset = doc_offset;
   write_doc_table(doc_buffer,de,false);
   free(de->URL);
   free(de);

   ptr++;
   char* start = ptr;
   char* prev = ptr;
   while(ptr < buf + len){
      if(!IS_ALPHANUM(*ptr)){
         if(ptr - prev > 0 && ptr - prev <= WORD_LENGTH_MAX && IS_ALPHANUM(*prev)){
            int L = ptr - prev + 1;
            char* term = (char *)malloc(L);
            memcpy(term, prev , L - 1);
            term[L-1] = 0;
            process_term(L - 1, term, doc_id, ptr - start); // extract term
            free(term);
         }
         ptr ++;
         prev = ptr;
      }else{
         if(!IS_ALPHANUM(*prev))
            prev = ptr;
         if(IS_UPPER(*ptr))
            *ptr += 'a' - 'A';
         ptr++;
      }
   }
   doc_id ++;
   return 0;
}

long term_id_counter = 0;
// counter for term id

void process_term(int len, char* term, int doc_id, int pos){
   map_entry* value = NULL;
   char flag = 1;
   if(len < WORD_LENGTH_MIN)
      return;
   // if(blacklist(term, len) == 1)
   //    return;
   map_entry* key = (map_entry*)malloc(sizeof(map_entry));
   memset(key->key_string, 0, sizeof(key->key_string));
   memcpy(key->key_string ,term, len);
   
   value = hashmap_get(term_map, key);
   free(key);
   if ( value ==  NULL|| value->te == NULL){
      term_entry* te = (term_entry*)malloc(sizeof(term_entry));

      if(value != NULL){
         flag = 0;
         te->term_id = value->number;
      }
      if(flag){
         value = (map_entry*)malloc(sizeof(map_entry));
         int term_id = term_id_counter;
         memcpy(value->key_string ,term, len);
         value->number = term_id;
         te->term_id = term_id;
         // printf("%s\n",term);
         term_id_counter++;

         write_term_id_map(term_id, term, len, term_id_counter);

         memory_allocated += sizeof(map_entry);
         // printf("%ld\n", memory_allocated);
      }

      value->te = te;
      te->tc_length = 1;
      te->total_size = sizeof(te->total_size) + sizeof(te->tc_length) + sizeof(te->term_id);

      term_chunk* tc = (term_chunk*)malloc(sizeof(term_chunk));
      tc->next = NULL;

      te->chunk_list_head = tc;
      te->chunk_list_tail = tc;

      tc->doc_id = doc_id;
      tc->freq = 1;
      te->total_size += sizeof(tc->doc_id) + sizeof(tc->freq);

      memory_allocated += sizeof(term_entry) + sizeof(term_chunk);
      if(flag){
         hashmap_set(term_map, value);
         free(value);
      }
   }else{
      // long term_id = value->number;
      term_entry* te = value->te;
      if(te->chunk_list_tail->doc_id == doc_id){
         term_chunk* tc = te->chunk_list_tail;
         tc->freq ++;
         
      }else{
         term_chunk* tc = (term_chunk*)malloc(sizeof(term_chunk));
         tc->next = NULL;

         te->chunk_list_tail->next = tc;
         te->chunk_list_tail = tc;
         te->tc_length ++;

         tc->doc_id = doc_id;
         tc->freq = 1;
         te->total_size += sizeof(tc->doc_id) + sizeof(tc->freq);
         memory_allocated += + sizeof(term_chunk);
      }
   }
}

int num_intermediate_files = 0;

// break intermediate files by MEMORY_LIMIT
int iterate_term_map(int flag){
   char filename[1024];
   // hashmap_iterate(term_map,&handle_map_entry,stdout);
   if(memory_allocated >= MEMORY_LIMIT_SETTING || flag == 1){
      snprintf(filename, sizeof(filename), "%s-%d", "tmp/intermediate", num_intermediate_files);
      FILE* f= fopen(filename, "w+");
      file_buffer* fb = init_dynamic_buffer(OUTPUT_BUFFER);
      fb -> f = f;
      hashmap_scan(term_map, &handle_map_entry,&fb);
      write_record(fb, NULL ,true);
      fclose(f);
      free_file_buffer(fb);
      hashmap_scan(term_map,&_free,0);
      memory_allocated = 0;
      num_intermediate_files ++;
      printf("Successfully created %s...\n",filename);
   }
   if(num_intermediate_files == MAX_INTERMEDIATE)
      return -1;
   return 0;
}


long progress = 0; // loaded size from input file

// parse
void parse(FILE* f_in, int percentage){
   term_map = hashmap_new(sizeof(map_entry), 0, 0, 0, 
                                    user_hash, user_compare, NULL);
   fseek(f_in, 0L, SEEK_END);
   long sz = ftell(f_in);
   fseek(f_in, 0L, SEEK_SET);

   if(percentage < 10){
      sz = sz/10 * percentage;
   }

   init_term_id_map();
   file_buffer* fb_in = init_dynamic_buffer(INPUT_BUFFER);
   char* input_buffer = fb_in->buffer;
   fb_in->f = f_in;
   doc_buffer = init_dynamic_buffer(2000000);
   doc_buffer->f = fopen("output/doc_table","w+");
   fseek(doc_buffer->f,4, SEEK_SET);
   
   int readed;

   while((readed=fread(input_buffer,sizeof(char),INPUT_BUFFER-1,fb_in->f)) > 0 && progress < sz) {
      char* ptr = input_buffer;
      bool flag = false;
      while(ptr < input_buffer + readed){
         char* s = strstr(ptr, "<TEXT>\n");
         if( s == NULL )
            break;
         s += strlen("<TEXT>\n");
         char* e = strstr(s,"</TEXT>");
         if( e == NULL )
            break;
         
         ptr = e + strlen("</TEXT>\n") + 1;
         if (s[0]!= 'h')
            continue;
         long doc_offset = ftell(fb_in->f) - readed + (s - input_buffer);
         process_document( e - s - 1, s, doc_offset);
         if(iterate_term_map(0) == -1){
            flag = true;
            break;
         }
      }
      progress += readed;
      double perc = (double)progress/sz*100;
      if(progress >= sz)
         perc = 100.0;
      printf("|\t%-9.2lf%%\t|\t%-6ld\t\t\t|\n",perc,term_id_counter);
      if(flag == true)
         break;
   } 
   
   fclose(f_in);

   iterate_term_map(1);
   flush_term_to_file(true, term_id_counter);

   printf("Total terms: %ld\n", term_id_counter);
   printf("Total documents: %ld\n", doc_id);
   
   write_doc_table(doc_buffer,NULL,true);
   write_doc_table_end(doc_buffer, doc_id);
   free_file_buffer(doc_buffer);
   
   hashmap_scan(term_map,&_free,0);
   // hashmap_scan(term_map,&_free_hashmap,0);
   hashmap_free(term_map);

}

/*  End of Document Parser  */


/*  CLI  */

char *help_message = "-b <TREC file>  build inverted index table, lexicon and doc table from input file(without <>). Output to output/... \n"
             "-q [TREC file] Query mode. Set TREC file if you want to show snippet. \n"
             "-p <1~10> Load 10 percent to 100 percent of data, mapping to 1 to 10 \n"
             "-m <400~8000> Memory Limit 400MB to 8GB, mapping to 400 to 8000\n"; 

void help(){
   printf("%s\n",help_message);
   exit(EXIT_SUCCESS);
}

// build command
void b(char* filename, int percentage){
   
   FILE* f_in = fopen(filename, "r");
   if(f_in == NULL){
      printf("Fail to open TREC file. Maybe does not exist or with wrong permission.\n");
      exit(EXIT_FAILURE);
   }

   struct stat st = {0};
   if (stat("output/", &st) == -1) {
      mkdir("output/", 0700);
   }
   if (stat("tmp/", &st) == -1) {
      mkdir("tmp/", 0700);
   }

   remove_files("tmp/intermediate");
   remove_files("tmp/sorted");
   remove_files("tmp/merged");
   remove("tmp/merged-0");
   remove("tmp/tmp_term");

   parse(f_in, percentage);
   fclose(f_in);

   merge_sort(MEMORY_LIMIT_SETTING);

   build();
   remove("tmp/merged");
   remove("tmp/tmp_term");
   printf("All success. You can use -q to query words.\n\n");
   exit(EXIT_SUCCESS);
}

//query command
void q(char* doc_path){

   if(init_query_database() == false){
      printf("Please build first!\n");
      exit(EXIT_FAILURE);
   }

   int mode = -1;

   int limit = -1;

   while( limit <= 0){
      printf("\nEnter result limit: \n");
      scanf("%d",&limit);
   }
   getchar();
   while( mode != CONJUNCTIVE_MODE && mode != DISJUNCTIVE_MODE){
      printf("Choose mode: [%d] Conjunctive mode. [%d] Disjunctive mode.\n", CONJUNCTIVE_MODE, DISJUNCTIVE_MODE);
      scanf("%d", &mode);
   }

   size_t len = 1024*1024;
   char* stdin_buf = (char*) malloc(len);
   int read;
   printf("\nEnter your query:\n");
   
   getchar();

   // FILE* test = fopen("test2.txt","r");
   while((read = getline(&stdin_buf, &len, stdin)) > 0){
      char** queries = (char**) malloc(read*sizeof(char*));
      int i = 0;
      int word_cnt = 0;

      while( i < read ){
         if(IS_ALPHANUM(stdin_buf[i])){
            int s = i;
            while(IS_ALPHANUM(stdin_buf[i])){
               if(IS_UPPER(stdin_buf[i]))
                  stdin_buf[i]+= 'a' - 'A';
               i++;
            }
            int L = i - s + 1;
            queries[word_cnt] = (char*) malloc(L);
            memset(queries[word_cnt], 0, L);
            memcpy(queries[word_cnt], stdin_buf + s, L - 1);
            word_cnt++;
         }
         i++;
      }

      if(word_cnt == 0){
         free(queries);
         break;
      }
      
      query(queries, word_cnt, limit, mode, doc_path);

      for(i = 0; i < word_cnt; i++){
         free(queries[i]);
      }
      free(queries);

      // break;
      printf("\nEnter your query:\n");

   };
   free(stdin_buf);
   close_query_database();
   exit(0);
}

void __exit(){
   help();
   exit(0);
}

int main(int argc, char *argv[])
{
   if(argc <= 1 || strlen(argv[1]) <= 1 || argv[1][0] != '-')
      help();
   int p = 10;
   // int K = 10;
   char flag = 0;
   char* content;
   char* doc_path = NULL;

   for(int i=1;i < argc;i++){  
		if(argv[i][0]=='-'){
			switch(argv[i][1]){
            case 'b': 
               if(i+1 >=argc)
                  __exit();
               printf("|\t%-9s\t|\t%-2s\t|\n","Progress","Identified Terms");
               flag = 'b';
               content = argv[++i];
               break;
            case 'q':
               flag = 'q';
               if( ++i < argc && argv[i][0] != '-')
                  doc_path = argv[i];
               
               // if(++i < argc){
               //    K = atoi(argv[i]);
               //    if(K <= 0)
               //       __exit();
               // }
               break;
            case 'p':
               if(i+1 >=argc)
                  __exit();
               p = atoi(argv[++i]);
               if(p < 1 || p > 10){
                  printf("must between 1 and 10\n");
                  __exit();
               }
               break;
            case 'm':
               if(i+1 >=argc)
                  __exit();
               MEMORY_LIMIT_SETTING = atoi(argv[++i]);
               if(MEMORY_LIMIT_SETTING < 400 || MEMORY_LIMIT_SETTING > 8000){
                  printf("must between 400MB and 8000MB\n");
                  exit(0);
               }
               MEMORY_LIMIT_SETTING *= 1024*1024;
               MEMORY_LIMIT_SETTING = (long)(MEMORY_LIMIT_SETTING);
               break;
            
            default:break;
         }
		}else
			break;
	}
   switch (flag)
   {
      case 'b': b(content, p);
         break;
      case 'q': q(doc_path);
         break;
      default:
         break;
   }
   __exit();
}


