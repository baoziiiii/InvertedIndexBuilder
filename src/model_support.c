#include <stdbool.h>
#include <unistd.h> 
#include <fcntl.h> 
#include <stdio.h>
#include <string.h>
#include "model.h"
#include "merge_sort.h"

/*  Dynamic Buffer  */
file_buffer* init_dynamic_buffer(int size){
    file_buffer* fb = (file_buffer*)malloc(sizeof(file_buffer));
    fb->size = size;
    fb->curr = 0;
    fb->max = 0;
    fb->buffer = (char*)malloc(fb->size);
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
        tc->doc_id = doc_id;
        tc->next = NULL;

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

        for(int j = 0; j < freq; j++){
            int position = -1;
            memcpy(&position, fb->buffer + fb->curr , sizeof(int));
            fb->curr += sizeof(int);

            position_node* pn = (position_node*)malloc(sizeof(position_node));
            pn->position = position;
            pn->next = NULL;

            if( j == 0){
                tc->head = pn;
                tc->tail = pn;
            }else{
                tc->tail->next = pn;
                tc->tail = pn;
            }
        }
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

        position_node* pos_ptr = ptr->head;

        while(pos_ptr){
            memcpy(fb -> buffer + fb->curr , &pos_ptr->position ,sizeof(int));
            fb->curr  += sizeof(int);
            pos_ptr = pos_ptr->next;
        }
        ptr = ptr->next;
    }

    return true;
}


int write_to_final_inverted_list(file_buffer* fb, term_entry*te, bool flush){
    static int offset = 0;
    if(flush == true){
        flush_buffer_to_file(fb);
        fb->curr  = 0;
    }
    if( te == NULL )
        return -1;

    int l = te->total_size - sizeof(te->term_id) - sizeof(te->total_size);
    if ( l + fb->curr  >= fb -> size )
        flush_buffer_to_file(fb);
    
    memcpy(fb -> buffer + fb->curr , &te->tc_length,sizeof(int) );
    fb->curr  += sizeof(int);
    term_chunk* ptr = te->chunk_list_head;

    while(ptr){
        memcpy(fb -> buffer + fb->curr , &ptr->doc_id ,sizeof(int) );
        fb->curr  += sizeof(int);
        memcpy(fb -> buffer + fb->curr , &ptr->freq ,sizeof(int) );
        fb->curr  += sizeof(int);
        position_node* pos_ptr = ptr->head;

        while(pos_ptr){
            memcpy(fb -> buffer + fb->curr , &pos_ptr->position ,sizeof(int));
            fb->curr  += sizeof(int);
            pos_ptr = pos_ptr->next;
        }
        ptr = ptr->next;
    }
    offset += l;
    
    return offset - l;

}


void read_inverted_list(FILE* f, int offset, int length, term_entry* te){
    int doc_len = 0;
    int freq = 0;
    int position = -1;

    fseek(f, offset, SEEK_SET);

    fread(&doc_len, sizeof(int), 1, f);

    te->tc_length = doc_len;

    for( int i = 0;i < doc_len; i++){

        term_chunk* tc = (term_chunk*)malloc(sizeof(term_chunk));
        fread(&tc->doc_id, sizeof(int), 1, f);
        tc->next = NULL;

        if( i == 0){
            te->chunk_list_head = tc;
            te->chunk_list_tail = tc;
        }else{
            te->chunk_list_tail->next = tc;
            te->chunk_list_tail = tc;
        }

        fread(&freq, sizeof(int), 1, f);
        tc->freq = freq;

        for(int j = 0; j < freq; j++){
            fread(&position, sizeof(int), 1, f);

            position_node* pn = (position_node*)malloc(sizeof(position_node));
            pn->position = position;
            pn->next = NULL;

            if( j == 0){
                tc->head = pn;
                tc->tail = pn;
            }else{
                tc->tail->next = pn;
                tc->tail = pn;
            }
        }
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

        position_node* src_pn = src_tc->head;
        for(int j = 0; j < tc->freq; j++){
            position_node* pn = (position_node*)malloc(sizeof(position_node));
            pn -> position = src_pn -> position;
            pn -> next = NULL;
            if( j == 0){
                tc -> head = pn;
                tc -> tail = pn;
            }else{
                tc -> tail -> next = pn;
                tc -> tail = pn;
            }
            src_pn = src_pn -> next;
        }
        
        dst->chunk_list_tail->next = tc;
        dst->chunk_list_tail = tc;
        src_tc = src_tc -> next;
    }
    dst->tc_length += src->tc_length;
    dst->total_size += src->total_size - sizeof(src->tc_length) - sizeof(src->term_id) - sizeof(src->total_size);
    return true;
}


void write_doc_table(doc_entry *de){
   // printf("%s\n",de->URL);
   int fd = open("output/doc_table.txt", O_CREAT|O_RDWR|O_APPEND,S_IWUSR|S_IRUSR);
   dprintf(fd,"[%d]\t\t%d\n\t%s\n",de->doc_id,de->length, de->URL);
   close(fd);
}


void print_term_entry(term_entry* te){
    // printf("[%d]\n",te->term_id);
    term_chunk* tc = te->chunk_list_head;
    for(int i = 0; i < te->tc_length; i++){
        int doc_id = tc->doc_id;
        printf("\tdoc_id[%d]\n", doc_id);
        position_node* pn = tc->head;
        for(int j = 0; j < tc->freq; j++){
            printf("\t\t%d\n", pn->position);
            pn = pn -> next;
        }
        tc = tc -> next;
    }
}

bool _free_te(term_entry* te){
   if(te == NULL)
      return true;
   term_chunk* ptr = te->chunk_list_head;
   while(ptr){
      position_node* pos_ptr = ptr->head;
      while(pos_ptr){
         position_node* tmp = pos_ptr->next;
         free(pos_ptr);
         pos_ptr = tmp;
      }
      term_chunk* tmp = ptr->next;
      free(ptr);
      ptr = tmp;
   }
   free(te);
   return true;
}
