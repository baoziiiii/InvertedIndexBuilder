#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> 
#include <unistd.h> 
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stddef.h>
#include "model.h"
#include "hashmap.h"
#include "merge_sort.h"
#include "termid.h"
#include "lexicon.h"
#include "final_build.h"

#define IS_LOWER(x)     ((x) <= 'z' && (x) >= 'a')  
#define IS_UPPER(x)     ((x) <= 'Z' && (x) >= 'A')
#define IS_ALPHA(x)     (IS_LOWER(x) || IS_UPPER(x))


// ./bin/main -b /Users/baowenqiang/GoogleDataSet/msmarco-docs.trec
// ./bin/main -q word

// valgrind --tool=memcheck --leak-check=yes --log-file=memchk.log --show-reachable=yes --num-callers=20 --track-fds=yes ./bin/main -b /Users/baowenqiang/GoogleDataSet/msmarco-docs.trec

// memory limit for hashmap
long MAP_MEMORY_LIMIT = (long)(1000000000*0.8);

// Limit # of intermediate temporary file produced by parsing 
#define MAX_INTERMEDIATE 100

// buffer limit for read_input 


// max word length <= 
#define WORD_LENGTH_LIMIT 13


#define KEY_MAX_LENGTH (WORD_LENGTH_LIMIT+1)
// const char *stopwords[] = {"i", "me", "my", "myself", "we", "our", "ours", "ourselves", "you", "your", "yours", "yourself", "yourselves", "he", "him", "his", "himself", "she", "her", "hers", "herself", "it", "its", "itself", "they", "them", "their", "theirs", "themselves", "what", "which", "who", "whom", "this", "that", "these", "those", "am", "is", "are", "was", "were", "be", "been", "being", "have", "has", "had", "having", "do", "does", "did", "doing", "a", "an", "the", "and", "but", "if", "or", "because", "as", "until", "while", "of", "at", "by", "for", "with", "about", "against", "between", "into", "through", "during", "before", "after", "above", "below", "to", "from", "up", "down", "in", "out", "on", "off", "over", "under", "again", "further", "then", "once", "here", "there", "when", "where", "why", "how", "all", "any", "both", "each", "few", "more", "most", "other", "some", "such", "no", "nor", "not", "only", "own", "same", "so", "than", "too", "very", "s", "t", "can", "will", "just", "don", "should", "now"};


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

bool _free_hashmap(const void * value, void * x){
   /* Free all of the values we allocated and remove them from the map */
   value = hashmap_delete(term_map, ((map_entry*)value)->key_string);
   // if(value != NULL)
   //    free(value);
   return true;
}

/* End of hash map */


/*  Document Parser  */

// memory allocated for hashmap entries 
long memory_allocated = 0; 

void process_term(int len, char* term, int doc_id, int pos);

// parse document 
int process_document(int len, char* buf){

   static int doc_id = 0;

   doc_entry *de = (doc_entry *)malloc(sizeof(doc_entry));
   de->doc_id = doc_id;

   char* ptr = strchr(buf, '\n');
   int len_url = ptr - buf + 1;
   de->URL = (char *)malloc(len_url);
   memcpy(de->URL, buf, len_url - 1);
   de->URL[len_url - 1] = 0;
   de->length = len;
   write_doc_table(de);
   free(de->URL);
   free(de);

   ptr++;
   char* start = ptr;
   char* prev = ptr;
   while(ptr < buf + len){
      if(!IS_ALPHA(*ptr)){
         if(ptr - prev > 0 && ptr - prev <= WORD_LENGTH_LIMIT && IS_ALPHA(*prev)){
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
         if(!IS_ALPHA(*prev))
            prev = ptr;
         if(IS_UPPER(*ptr))
            *ptr += 'a' - 'A';
         ptr++;
      }
   }
   doc_id ++;
   return 0;
}

// int strcicmp(char const *a, char const *b, int n)
// {
//     for (int i = 0; i < n; i++) {
//         int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
//         if (d != 0 || !*a)
//             return d;
//          a++;
//          b++;
//     }
//     return 0;
// }

// int blacklist(char* s, int len){
//    if(len < 2)
//       return 1;
//    for(int i = 0; i < sizeof(stopwords)/sizeof(stopwords[0]); i++){
//       int min = strlen(stopwords[i]) < len ? strlen(stopwords[i]): len;
//       if(strcicmp(stopwords[i],s, min) == 0)
//          return 1;
//    }
//    return 0;
// }

long term_id_counter = 0;
// counter for term id

void process_term(int len, char* term, int doc_id, int pos){
   map_entry* value = NULL;
   char flag = 1;
   if(len < 2)
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
      te->chunk_list_head = tc;
      te->chunk_list_tail = tc;

      tc->doc_id = doc_id;
      tc->freq = 1;
      te->total_size += sizeof(tc->doc_id) + sizeof(tc->freq);

      tc->head = (position_node*)malloc(sizeof(position_node));
      tc->head->position = pos;
      tc->head->next = NULL;
      tc->tail = tc->head;
      tc->next = NULL;
      te->total_size += sizeof(pos);

      memory_allocated += sizeof(term_chunk)+sizeof(term_chunk)+sizeof(position_node);
      if(flag){
         hashmap_set(term_map, value);
         free(value);
      }
   }else{
      // long term_id = value->number;
      term_entry* te = value->te;
      if(te->chunk_list_tail->doc_id == doc_id){
         term_chunk* tc = te->chunk_list_tail;
         position_node* pn = (position_node*)malloc(sizeof(position_node));
         pn->position = pos;
         pn->next = NULL;
         
         tc->freq ++;
         tc->tail->next = pn;
         tc->tail = pn;
         te->total_size += sizeof(pos);
         memory_allocated += sizeof(position_node);
      }else{
         term_chunk* tc = (term_chunk*)malloc(sizeof(term_chunk));

         te->chunk_list_tail->next = tc;
         te->chunk_list_tail = tc;
         te->tc_length ++;

         tc->doc_id = doc_id;
         tc->freq = 1;
         tc->head = (position_node*)malloc(sizeof(position_node));
         tc->head->position = pos;
         tc->head->next = NULL;
         tc->tail = tc->head;
         tc->next = NULL;
         te->total_size += sizeof(tc->doc_id) + sizeof(tc->freq) + sizeof(pos);
         memory_allocated += sizeof(position_node) + sizeof(term_chunk);
      }
   }
}

int num_intermediate_files = 0;

// break intermediate files by MEMORY_LIMIT
int iterate_term_map(int flag){
   char filename[1024];
   // hashmap_iterate(term_map,&handle_map_entry,stdout);
   if(memory_allocated >= MAP_MEMORY_LIMIT || flag == 1){
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
char input_buffer[INPUT_BUFFER];

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
   
   input_buffer[INPUT_BUFFER-1] = 0;
   int readed;
   while((readed=fread(input_buffer,sizeof(char),INPUT_BUFFER-1,f_in)) > 0 && progress < sz) {
      char* ptr = input_buffer;
      bool flag = false;
      while(ptr < input_buffer + INPUT_BUFFER){
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
         process_document( e - s - 1, s);
         if(iterate_term_map(0) == -1){
            flag = true;
            break;
         }
      }
      progress += readed;
      double perc = (double)progress/sz*100;
      if(progress >= sz)
         perc = 100.0;
      printf("|\t%-9.2lf%%\t|\t%-6ld\t\t|\n",perc,term_id_counter);
      if(flag == true)
         break;
   } 
   
   fclose(f_in);

   iterate_term_map(1);
   flush_term_to_file(true, term_id_counter);
   
   hashmap_scan(term_map,&_free,0);
   hashmap_scan(term_map,&_free_hashmap,0);
   hashmap_free(term_map);

}

/*  End of Document Parser  */


/*  CLI  */

char *help_message = "-b <TREC file>  build inverted index table, lexicon and doc table from input file(without <>). Output to output/... \n"
             "-q <word> query a word(without <>).\n"
             "-p <1~10> Load 10 percent to 100 percent of data, mapping to 1 to 10 \n"
             "-m <400~8000> Memory Limit 400MB to 8GB, mapping to 400 to 8000\n"; 

void help(){
   printf("%s",help_message);
   exit(EXIT_SUCCESS);
}

// build command
void b(char* filename, int percentage){
   FILE* f_in = fopen(filename, "r");
   parse(f_in, percentage);
   fclose(f_in);

   merge_sort(num_intermediate_files);
   printf("Building merged file...\n");
   build();
   remove("tmp/merged");
   remove("tmp/tmp_term");
   printf("All done. See output directory.\n\n");
   help();
   exit(EXIT_SUCCESS);
}

//query command
void q(char* s){
   if(read_lexicon_file() == false){
      printf("Please build first!\n");
      exit(EXIT_FAILURE);
   }
   char* ptr = s;
   while(*ptr != '\0'){
      if(IS_UPPER(*ptr))
         *ptr += 'a' - 'A';
      ptr++;
   }
   if(query(s) == false)
      printf("No such word in lexicon.\n");
   close_lexicon_file();
   exit(EXIT_SUCCESS);
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
   char flag = 0;
   char* content;
   for(int i=1;i < argc;i++){  
		if(argv[i][0]=='-'){
			switch(argv[i][1]){
            if(i+1 >=argc)
               __exit();
            case 'b': 
               printf("|\t%-9s\t|\t%-2s\t|\n","Loaded","total words");
               flag = 'b';
               content = argv[++i];
               break;
            case 'q':
               flag = 'q';
               content = argv[++i];
               break;
            case 'p':
               p = atoi(argv[++i]);
               if(p < 1 || p > 10){
                  printf("must between 1 and 10\n");
                  __exit();
               }
               break;
            case 'm':
               MAP_MEMORY_LIMIT = atoi(argv[++i]);

               if(MAP_MEMORY_LIMIT < 400 || MAP_MEMORY_LIMIT > 8000){
                  printf("must between 400MB and 8000MB\n");
                  exit(0);
               }
               MAP_MEMORY_LIMIT *= 1024*1024;
               MAP_MEMORY_LIMIT = (long)(MAP_MEMORY_LIMIT*0.8);
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
      case 'q': q(content);
      default:
         break;
   }

   __exit();
}


