#include <stdio.h>
#include <string.h>
#include <math.h>
#include "query.h"
#include "lexicon.h"
#include "model.h"
#include "heap.h"
#include "inverted_list.h"

/*  Lexicon Map  */
// read from lexicon to a map in memory , used for query
struct hashmap * lexicon_map;

uint64_t l_hash(const void *item, uint64_t seed0, uint64_t seed1) {
    const lexicon_el* data = item;
    return hashmap_sip(data->key_string, strlen(data->key_string), seed0, seed1);
}

int l_compare(const void *a, const void *b, void *udata) {
    const lexicon_el *ua = a;
    const lexicon_el *ub = b;
    return strcmp(ua->key_string, ub->key_string);
}

doc_entry** doc_table; // doc_entry array
int maxdocID = 0;
double d_avr = 0; 

bool read_doc_file(){
    FILE* f_doc = fopen("output/doc_table","r");
    file_buffer* fb = init_dynamic_buffer(100000000);
    fb -> f = f_doc;

    if(f_doc == NULL)
        return false;
    int L = 0;
    fread(&L, sizeof(int), 1, f_doc);
    doc_table = (doc_entry **)malloc(L*sizeof(doc_entry*));

    long sum_doc_size = 0;
    for(int i = 0; i < L; i++){
        doc_entry* de = (doc_entry*)malloc(sizeof(doc_entry));
        next_doc(fb,de);
        doc_table[de->doc_id] = de;
        sum_doc_size += de->size_of_doc;
    }
    maxdocID = L;
    d_avr = (double)sum_doc_size/(double)(L-1);
    free_file_buffer(fb);
    fclose(f_doc);
    return true;
}

void free_doc_table(){
    for(int i = 0; i < maxdocID; i++){
        free(doc_table[i]->URL);
        free(doc_table[i]);
    }
    free(doc_table);
}

bool init_query_database(){
    printf("Preparing...Do not enter anything\n");
    lexicon_map = hashmap_new(sizeof(lexicon_el), 0, 0, 0, 
                                l_hash, l_compare, NULL);
    
    if(read_lexicon_file(lexicon_map) == false || read_doc_file() == false)
        return false;
    printf("Done.\n");
    return true;
}


void close_query_database(){
    free_doc_table();
    // hashmap_scan(lexicon_map, &_free_lex, NULL);
    hashmap_free(lexicon_map);
}

double calculate_BM25(int freq, int ft,int doc_size){
    double k1 = 1.2;
    double b = 0.75;
    double BM25 = 0;
    BM25 = log2((maxdocID - ft + 0.5)/(ft + 0.5))*(k1+1)*freq/(k1*((1-b)+b*doc_size/d_avr)+freq);
    return BM25;
}

minHeap* init_heap(int size){
    return initMinHeap(size);
}

void to_rank_heap(minHeap* hp, int doc_id, double bm25, int limit){
    if(hp->size < limit){
        db* nd = (db*) malloc(sizeof(db));
        nd->doc_id = doc_id;
        nd->key = bm25 ;
        insertNode(hp, nd);
    }else{
        if(hp->elem[0]->key < bm25){
            db* pop = deleteNode(hp);
            free(pop);
            db* nd = (db*) malloc(sizeof(db));
            nd->doc_id = doc_id;
            nd->key = bm25 ;
            insertNode(hp, nd);
        }
    }
}


void out_rank_heap(minHeap* hp, db** results){
    while(hp->size){
        int i = hp->size;
        db* pop = deleteNode(hp);
        results[i - 1] = pop;
    }
}


int lvComparator(const void * e1, const void * e2){ return (*(IV** )e1)->length - (*(IV** )e2)->length ;} 

void disjunctive_query(minHeap* rankhp, IV** lps, int cnt, int limit){

    minHeap* lphp = init_heap(0);

    for(int i = 0; i < cnt; i++){
        db* nd = (db*) malloc(sizeof(db));
        nd -> doc_id = nextGEQ(lps[i],0);
        nd -> lp = lps[i] ;
        insertNode(lphp, nd);
    }
    while( lphp -> size){
        int top_doc = lphp->elem[0] -> doc_id;
        double bm25 = 0;
        while( lphp -> size && lphp->elem[0]->doc_id == top_doc){
            db* pop = deleteNode(lphp);
            IV* lp = pop->lp;
            bm25 += calculate_BM25(getFreq(lp), lp->length, doc_table[pop->doc_id]->size_of_doc);
            free(pop);
            int next_doc = nextGEQ(lp, top_doc);
            if( next_doc < maxdocID){
                db* nd = (db*) malloc(sizeof(db));
                nd -> doc_id = next_doc;
                nd -> lp = lp;
                insertNode(lphp, nd); 
            }
        }
        to_rank_heap(rankhp, top_doc, bm25, limit);
    }
    deleteMinHeap(lphp);
}

void conjunctive_query(minHeap* hp, IV** lps, int cnt, int limit){

    int did = 0;
    int d = 0;
    while(did < maxdocID){
        did = nextGEQ(lps[0], did);
        if(did >= maxdocID)
            break;
        for(int i = 1; i < cnt && ((d = nextGEQ(lps[i],did)) == did);i++);
        if( d > did) did = d;
        else{
            double bm25 = 0;
            for(int i = 0; i < cnt; i++){
                int fp = getFreq(lps[i]);
                int ft = lps[i]->length;
                bm25 += calculate_BM25(fp, ft, doc_table[did]->size_of_doc);
            }
            to_rank_heap(hp, did, bm25, limit);
            did++;
        }
    }
}
#define SNIPPET_LENGTH 512

bool generate_snippet(char* snippet, int doc_id, char** terms, int N, char* doc_path){
    
    doc_entry* de = doc_table[doc_id];
    if(doc_path == NULL){
        memcpy(snippet, de->URL, strlen(de->URL));
        return true;
    }
    int buffer_len = (de->size_of_doc < 1000000)?de->size_of_doc:1000000;
    file_buffer* fb_in = init_dynamic_buffer(buffer_len);
    fb_in->f = fopen(doc_path, "r");
    fseek(fb_in->f, de->offset, SEEK_SET);
    int readed;
    char* input_buffer = fb_in->buffer;

    readed=fread(input_buffer,sizeof(char), buffer_len, fb_in->f);
    char* ptr = strchr(input_buffer, '\n');
    ptr++;

    int max_score = 0;

    for( char* start = ptr; start + SNIPPET_LENGTH < input_buffer + readed; start+= SNIPPET_LENGTH){
        ptr = start;
        int score = 0;
        char* prev = ptr;

        while(ptr < start + SNIPPET_LENGTH ){
            if(!IS_ALPHANUM(*ptr)){
                if(ptr - prev > 0 && ptr - prev <= WORD_LENGTH_MAX && IS_ALPHANUM(*prev)){
                    int L = ptr - prev + 1;
                    char* term = (char *)malloc(L);
                    memset(term, 0 ,L);
                    memcpy(term, prev , L - 1);
                    for( int i = 0; i < N; i++){
                        if(strcmp(term,terms[i])== 0)
                            score++;
                    }
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
        if(score > max_score)
            memcpy(snippet,start, SNIPPET_LENGTH-1);
    }
    return true;
}

// query the lexicon map for offset in inverted index
bool query(char** terms, int N, int limit, int MODE, char* doc_path){
    if(doc_path != NULL){
        FILE* f = fopen(doc_path, "r");
        if(f == NULL){
            printf("Fail to open TREC file. Maybe does not exist or with wrong permission.\n");
            exit(EXIT_FAILURE);
        }
        fclose(f);
    }
 
    lexicon_el* key =(lexicon_el*) malloc(sizeof(lexicon_el));
    lexicon_el* value;
    char snippet[SNIPPET_LENGTH] = {0};

    IV** lps = (IV**) malloc(N*sizeof(IV*));
    int cnt = 0;
    for(int i = 0; i < N; i++){
        memset(key->key_string, 0, WORD_LENGTH_MAX);
        memcpy(key->key_string, terms[i], strlen(terms[i]));
        key->key_string[WORD_LENGTH_MAX - 1] = 0;
        value = hashmap_get(lexicon_map,key);
        if(value == NULL){
            ;
            // printf("%s Not Exist\n",key->key_string);
        }else{
            lps[cnt] = openList(value->offset, maxdocID);
            cnt ++;
        }   
    }
    
    minHeap* hp = init_heap(0);

    qsort(lps, cnt, sizeof(IV*), lvComparator);

    if(cnt > 0){
        switch(MODE){
            case CONJUNCTIVE_MODE:conjunctive_query(hp,lps,cnt,limit); break;
            case DISJUNCTIVE_MODE:disjunctive_query(hp,lps,cnt,limit); break;
        }
    }

    int L = hp -> size;
    db** results = (db**) malloc(L*sizeof(db*));
    out_rank_heap(hp, results);
    printf("\n ----------------------------------------\n");
    printf("|        Showing Top %d Result           |\n",limit);
    printf(" ----------------------------------------\n\n");
    if( L == 0){
        printf("No result found.\n");
    }
    for(int i = 0; i < L; i++){
        generate_snippet(snippet, results[i]->doc_id, terms, N, doc_path);
        if(doc_path == NULL){
            printf("[%d]\t%s\nBM25: \t%f\n\n",i + 1,doc_table[results[i]->doc_id]->URL, results[i]->key);
        }else{
            printf("[%d]\t%s\nBM25: \t%f\n<Snippet>\n%s\n</Snippet>\n\n",i + 1,doc_table[results[i]->doc_id]->URL, results[i]->key,  snippet);
        }
        free(results[i]);
    }
    
    // deleteMinHeap(hp);

    free(key);
    for(int i = 0; i < cnt; i++)
        closeList(lps[i]);

    free(lps);
    return true;
}