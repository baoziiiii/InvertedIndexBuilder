#include <stdbool.h>
#include <unistd.h> 
#include <fcntl.h> 
#include <stdio.h>
#include <string.h>
#include "model.h"
#include "merge_sort.h"
#include "var_bytes.h"

/*  Dynamic Buffer  */
file_buffer* init_dynamic_buffer(int size){
    file_buffer* fb = (file_buffer*)malloc(sizeof(file_buffer));
    fb->size = size;
    fb->curr = 0;
    fb->max = 0;
    fb->buffer = (char*)malloc(fb->size);
    memset(fb->buffer,0, fb->size);
    return fb;
}

void free_file_buffer(file_buffer* fb){
    free(fb->buffer);
    free(fb);
}

/*  batch read  */
void read_batch_to_file_buffer(file_buffer *fb){
    fb -> max = fread(fb->buffer, 1, fb->size,fb->f);
    fb -> curr = 0;
}

/*  read next term entry record from binary buffer  */
bool next_record_from_file_buffer(file_buffer* fb, term_entry* te){
    if(fb->curr >= fb->max)
        read_batch_to_file_buffer(fb);
    if(fb->max <= 0)
        return false;

    int total_size = 0;
    memcpy(&total_size, fb->buffer + fb->curr,sizeof(int));
    if(fb->curr + total_size >= fb->size){
        fseek(fb->f, - (fb->size - fb->curr),SEEK_CUR); // if the leftover in the buffer is not 
        read_batch_to_file_buffer(fb);
    }

    fb->curr += sizeof(int);
    int term_id = 0;
    memcpy(&term_id, fb->buffer + fb->curr, sizeof(int));
    fb->curr += sizeof(int);

    te -> total_size = total_size;
    te -> term_id = term_id;
    int doc_len = 0;
    memcpy(&doc_len, fb->buffer + fb->curr, sizeof(int));
    fb->curr += sizeof(int);

    te->tc_length = doc_len;


    for(int i = 0; i < doc_len; i++){
        int doc_id = 0;
        memcpy(&doc_id, fb->buffer + fb->curr, sizeof(int));
        fb->curr += sizeof(int);
        term_chunk* tc = (term_chunk*)malloc(sizeof(term_chunk));
        tc->next = NULL;
        tc->doc_id = doc_id;

        if( i == 0){
            te->chunk_list_head = tc;
            te->chunk_list_tail = tc;
        }else{
            te->chunk_list_tail->next = tc;
            te->chunk_list_tail = tc;
        }

        int freq = 0;
    
        memcpy(&freq, fb->buffer + fb->curr , sizeof(int));
        fb->curr += sizeof(int);

        tc->freq = freq;
    }

    return true;
}


void flush_buffer_to_file(file_buffer* fb){
    if( fb->f != NULL ) 
        fwrite(fb->buffer,1,fb->curr,fb->f);
    fb->curr = 0;
}

bool write_record(file_buffer* fb, term_entry* te, bool flush){
    if(flush == true){
        flush_buffer_to_file(fb);
        fb->curr = 0;
    }
    if( te == NULL )
        return false;

    int l = te->total_size;
    if ( l + fb->curr >= fb -> size )
        flush_buffer_to_file(fb);

    memcpy(fb -> buffer + fb->curr , &te->total_size,sizeof(int) );
    fb->curr  += sizeof(int);
    memcpy(fb -> buffer + fb->curr , &te->term_id,sizeof(int) );
    fb->curr  += sizeof(int);
    memcpy(fb -> buffer + fb->curr , &te->tc_length,sizeof(int) );
    fb->curr  += sizeof(int);

    term_chunk* ptr = te->chunk_list_head;

    while(ptr){
        memcpy(fb -> buffer + fb->curr , &ptr->doc_id ,sizeof(int) );
        fb->curr  += sizeof(int);
        memcpy(fb -> buffer + fb->curr , &ptr->freq ,sizeof(int) );
        fb->curr  += sizeof(int);

        ptr = ptr->next;
    }

    return true;
}


long write_to_final_inverted_list(file_buffer* fb, term_entry* te, bool flush){
    static int offset = 0;
    if(flush == true){
        flush_buffer_to_file(fb);
        fb->curr  = 0;
    }
    if( te == NULL )
        return -1;

    unsigned char* vb_bytes = NULL;
    int L = 0;

    int estimated_max_len = (te->total_size - sizeof(te->term_id) - sizeof(te->total_size))*2;
    if ( fb->curr + estimated_max_len  >= fb -> size )
        flush_buffer_to_file(fb);
    
    int actual_len = 0;

    vb_bytes = vb_encode(te->tc_length, &L);
    memcpy(fb -> buffer + fb->curr , vb_bytes , L);
    free(vb_bytes);

    fb->curr  += L;
    actual_len += L;

    term_chunk* ptr = te->chunk_list_head;
    int prev_doc_id = 0;

    while(ptr){

        int diff_doc_id = ptr->doc_id - prev_doc_id;
        prev_doc_id = ptr->doc_id;
        vb_bytes = vb_encode(diff_doc_id, &L);
        memcpy(fb -> buffer + fb->curr , vb_bytes, L );
        free(vb_bytes);
        fb->curr  += L;
        actual_len += L;

        vb_bytes = vb_encode(ptr->freq, &L);
        memcpy(fb -> buffer + fb->curr , vb_bytes , L );
        free(vb_bytes);

        fb->curr  += L;
        actual_len += L;

        ptr = ptr->next;
    }
    offset += actual_len;
    
    return offset - actual_len;
}

// go to the offset of inverted list and read the record
void read_inverted_list(FILE* f, int offset, int length, term_entry* te){

    fseek(f, offset, SEEK_SET);
    
    int doc_len = vb_decode_stream(f);
    // fread(&doc_len, sizeof(int), 1, f);

    te->tc_length = doc_len;
    
    int doc_id = 0;
    for( int i = 0;i < doc_len; i++){

        term_chunk* tc = (term_chunk*)malloc(sizeof(term_chunk));
        tc->next = NULL;

        doc_id = doc_id + vb_decode_stream(f);
        tc->doc_id = doc_id;
        // fread(&tc->doc_id, sizeof(int), 1, f);
        
        if( i == 0){
            te->chunk_list_head = tc;
            te->chunk_list_tail = tc;
        }else{
            te->chunk_list_tail->next = tc;
            te->chunk_list_tail = tc;
        }
        // fread(&freq, sizeof(int), 1, f);
        tc->freq = vb_decode_stream(f);
    }
}

bool merge_same_term(term_entry* dst, term_entry* src){
    if( dst == NULL || src == NULL || dst->term_id != src->term_id)
        return false;

    term_chunk* src_tc = src->chunk_list_head;

    for(int i = 0; i < src->tc_length; i++){
        term_chunk* tc = (term_chunk*)malloc(sizeof(term_chunk));
        tc->freq = src_tc->freq;
        tc->doc_id = src_tc->doc_id;
        tc->next = NULL;
        
        dst->chunk_list_tail->next = tc;
        dst->chunk_list_tail = tc;
        src_tc = src_tc -> next;
    }
    dst->tc_length += src->tc_length;
    dst->total_size += src->total_size - sizeof(src->tc_length) - sizeof(src->term_id) - sizeof(src->total_size);
    return true;
}


// void write_doc_table(doc_entry *de){
//    // printf("%s\n",de->URL);
//    int fd = open("output/doc_table.txt", O_CREAT|O_RDWR|O_APPEND,S_IWUSR|S_IRUSR);
//    dprintf(fd,"[%d]\t\t%d\n\t%s\n",de->doc_id,de->length, de->URL);
//    close(fd);
// }



void write_doc_table_end(file_buffer* fb, int total){
    fseek(fb->f, 0, SEEK_SET);
    int L = total;
    fwrite(&L, sizeof(L),1,fb->f);
    fclose(fb->f);
}

bool write_doc_table(file_buffer* fb, doc_entry* de, bool flush){
    if(flush == true){
        flush_buffer_to_file(fb);
        fb->curr = 0;
    }

    if( de == NULL )
        return false;

    int url_len = strlen(de->URL);
    if(url_len <= 0 || url_len >= 1000){
        printf("Invalid url len%d\n",url_len);
        url_len = 1;
    }

    int L = url_len + 3*sizeof(int);
    if (  L + fb->curr >= fb -> size )
        flush_buffer_to_file(fb);
    
    memcpy(fb -> buffer + fb->curr , &L ,sizeof(int) ); // length of this unit
    fb->curr  += sizeof(int);
    memcpy(fb -> buffer + fb->curr , &de->doc_id,sizeof(int) ); // doc_id
    fb->curr  += sizeof(int);
    memcpy(fb -> buffer + fb->curr , &de->size_of_doc,sizeof(int) ); //  size of this doc
    fb->curr  += sizeof(int);
    memcpy(fb -> buffer + fb->curr , de->URL, url_len ); // URL 
    fb->curr  += url_len;
    return true;
}


bool next_doc(file_buffer* fb,  doc_entry* de){
    if(fb->curr >= fb->max)
        read_batch_to_file_buffer(fb);

    if(fb->max <= 0)
        return false;

    int total_size = 0;
    memcpy(&total_size, fb->buffer + fb->curr,sizeof(int));
    if(fb->curr + total_size >= fb->size){
        fseek(fb->f, - (fb->size - fb->curr),SEEK_CUR); // if the leftover in the buffer is not 
        read_batch_to_file_buffer(fb);
    }
    int url_len = total_size - 3*sizeof(int);
    fb->curr += sizeof(int);
    memcpy(&de->doc_id, fb->buffer + fb->curr, sizeof(int));
    fb->curr += sizeof(int);

    memcpy(&de->size_of_doc, fb->buffer + fb->curr, sizeof(int));
    fb->curr += sizeof(int);

    de->URL = (char*) malloc(url_len + 1);
    memset(de->URL, 0 , url_len + 1);
    memcpy(de->URL, fb->buffer + fb->curr, url_len);
    fb->curr += url_len;
    
    return true;
}


void print_term_entry(doc_entry** doc_table ,term_entry* te){
    // printf("[%d]\n",te->term_id);
    term_chunk* tc = te->chunk_list_head;
    for(int i = 0; i < te->tc_length; i++){
        int doc_id = tc->doc_id;
        doc_entry* de = doc_table[doc_id];
        printf("\tFreq:[%d]\t%s\n", tc->freq, de->URL);
        tc = tc -> next;
    }
}

bool _free_te(term_entry* te){
   if(te == NULL)
      return true;
   term_chunk* ptr = te->chunk_list_head;
   while(ptr){
      term_chunk* tmp = ptr->next;
      free(ptr);
      ptr = tmp;
   }
   free(te);
   return true;
}
