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

    int prev_doc_id = -1;
    for(int i = 0; i < doc_len; i++){
        int doc_id = 0;
        memcpy(&doc_id, fb->buffer + fb->curr, sizeof(int));
        if(doc_id <= prev_doc_id)
            printf("???\n");
        prev_doc_id = doc_id;
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
   static long offset = 0;

    if(flush == true){
        flush_buffer_to_file(fb);
        fb->curr  = 0;
    }
    if( te == NULL )
        return -1;

    int cnt = 0;
    term_chunk* ptr2 = te->chunk_list_head;
    while(ptr2){
        ptr2 = ptr2->next;
        cnt ++;
    }
    int block_length = (te->tc_length - 1) / BLOCK_SIZE + 1;

    unsigned char* vb_bytes = NULL;
    int L = 0;

    term_chunk* ptr = te->chunk_list_head;
    int prev_doc_id = 0;
    char* block_lastdoc_table_buffer = (char*) malloc(block_length*sizeof(int));
    int curr1 = 0;

    char* block_size_table_buffer = (char*) malloc(block_length*sizeof(int));
    int curr2 = 0;

    char* block_doc_buffer = (char*) malloc(BLOCK_SIZE*sizeof(int));
    int curr3 = 0;

    char* block_freq_buffer = (char*) malloc(BLOCK_SIZE*sizeof(int));
    int curr4 = 0;
    
    int curr34 = 0;

    for(int i = 0; i < block_length; i++){

        curr3 = 0;
        curr4 = 0;
        for(int j = 0; j < BLOCK_SIZE && ptr != NULL; j++){
            int diff_doc_id = ptr->doc_id - prev_doc_id;
            prev_doc_id = ptr->doc_id;
            vb_bytes = vb_encode(diff_doc_id, &L);
            free(vb_bytes);
            curr3 += L;

            vb_bytes = vb_encode(ptr->freq, &L);
            free(vb_bytes);
            curr4 += L;
            ptr = ptr->next;
        }

        curr34 += curr3 + curr4;

        vb_bytes = vb_encode(prev_doc_id, &L);
        memcpy(block_lastdoc_table_buffer + curr1 , vb_bytes, L );
        free(vb_bytes);
        curr1 += L;

        vb_bytes = vb_encode(curr3 + curr4, &L);
        memcpy(block_size_table_buffer + curr2 , vb_bytes, L );
        free(vb_bytes);
        curr2 += L;
    }

    int size0 = 0;
    vb_bytes = vb_encode(te->tc_length, &size0);
    free(vb_bytes);

    int size1 = 0;
    vb_bytes = vb_encode(curr1, &size1);
    free(vb_bytes);

    int size2 = 0;
    vb_bytes = vb_encode(curr2, &size2);
    free(vb_bytes);

    int estimated_max_len = size0 + size1 + size2 + curr1 + curr2 + curr34;
    if ( fb->curr + estimated_max_len  >= fb -> size )
        flush_buffer_to_file(fb);

    int old_fb_curr = fb->curr;

    vb_bytes = vb_encode(te->tc_length, &size0);
    memcpy(fb -> buffer + fb->curr , vb_bytes, size0 );
    fb->curr += size0;
    free(vb_bytes);

    vb_bytes = vb_encode(curr1, &size1);
    memcpy(fb -> buffer + fb->curr , vb_bytes, size1 );
    fb->curr += size1;
    free(vb_bytes);

    vb_bytes = vb_encode(curr2, &size2);
    memcpy(fb -> buffer + fb->curr , vb_bytes, size2 );
    fb->curr += size2;
    free(vb_bytes);


    memcpy(fb -> buffer + fb->curr , block_lastdoc_table_buffer, curr1 );
    fb->curr += curr1;
           
    memcpy(fb -> buffer + fb->curr , block_size_table_buffer, curr2 );
    fb->curr += curr2; 

    prev_doc_id = 0;
    ptr = te->chunk_list_head;
    for(int i = 0; i < block_length; i++){
        curr3 = 0;
        curr4 = 0;
        for(int j = 0; j < BLOCK_SIZE && ptr != NULL; j++){
            int diff_doc_id = ptr->doc_id - prev_doc_id;
            prev_doc_id = ptr->doc_id;
            vb_bytes = vb_encode(diff_doc_id, &L);
            memcpy(block_doc_buffer + curr3 , vb_bytes, L );
            free(vb_bytes);
            curr3 += L;
            vb_bytes = vb_encode(ptr->freq, &L);
            memcpy(block_freq_buffer + curr4 , vb_bytes , L );
            free(vb_bytes);
            curr4 += L;
            ptr = ptr->next;
        }
        memcpy(fb -> buffer + fb->curr , block_doc_buffer, curr3 );
        fb->curr += curr3; 

        memcpy(fb -> buffer + fb->curr , block_freq_buffer, curr4 );
        fb->curr += curr4; 
    }


    free(block_doc_buffer);
    free(block_freq_buffer);
    free(block_size_table_buffer);
    free(block_lastdoc_table_buffer);

    long old_offset = offset;

    offset += fb->curr - old_fb_curr;
    
    return old_offset;
}


term_chunk* read_block_to_cache(IV* lp){
    fseek(lp->f, lp -> block_size_table_offset, SEEK_SET);
    for(int i = 0; i < lp->block_curr_cnt - 1; i++){
        lp -> block_offset += vb_decode_stream(lp->f);
    }
    int bo = lp -> block_offset;
    lp -> block_offset += vb_decode_stream(lp->f);
    lp -> block_size_table_offset = ftell(lp->f);
    lp -> block_curr_cnt = 0;

    term_chunk* dummy = (term_chunk*) malloc(sizeof(term_chunk));
    dummy->next = NULL;
    term_chunk* ptr = dummy;
    fseek(lp->f,bo,SEEK_SET);
    int doc_id = lp -> block_curr_sdoc;
    while(doc_id != lp -> block_curr_ldoc){
        term_chunk* node = (term_chunk*) malloc(sizeof(term_chunk));
        node->next = NULL;
        doc_id += vb_decode_stream(lp->f);
        node -> doc_id = doc_id;
        ptr -> next = node;
        ptr = node;
    }

    if( doc_id == 0 && lp->block_curr_ldoc == 0){
        term_chunk* node = (term_chunk*) malloc(sizeof(term_chunk));
        node->next = NULL;
        node -> doc_id = doc_id;
        dummy -> next = node;
    }

    ptr = dummy->next;
    while(ptr != NULL){
        ptr -> freq = vb_decode_stream(lp->f);
        ptr = ptr->next;
    }

    term_chunk* r = dummy -> next;
    free(dummy);
    return r;
}


bool merge_same_term(term_entry* te1, term_entry* te2){
    if( te1 == NULL || te2 == NULL || te1->term_id != te2->term_id)
        return false;

    term_entry* src = NULL;
    term_entry* dst = NULL;
    if (te1->chunk_list_head->doc_id > te2->chunk_list_head->doc_id){
        src = te1;
        dst= te2;
    }else{
        src = te2;
        dst = te1;
    }

    term_chunk* src_tc = src->chunk_list_head;
    term_chunk* dst_tc = dst->chunk_list_head;
    for(int i = 0; i < src->tc_length; i++){

        term_chunk* tc = (term_chunk*)malloc(sizeof(term_chunk));
        tc->freq = src_tc->freq;
        tc->doc_id = src_tc->doc_id;
        tc->next = NULL;
        
        while(dst_tc->next && dst_tc->next->doc_id < src_tc -> doc_id){
            dst_tc = dst_tc -> next;
        }
        term_chunk* tmp = dst_tc -> next;
        dst_tc -> next = tc;
        tc -> next = tmp;
        dst_tc = dst_tc->next;
        if(dst_tc -> next == NULL)
            dst -> chunk_list_tail = dst_tc;
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

    int L = url_len + 3*sizeof(int) + sizeof(long);
    if (  L + fb->curr >= fb -> size )
        flush_buffer_to_file(fb);
    
    memcpy(fb -> buffer + fb->curr , &L ,sizeof(int) ); // length of this unit
    fb->curr  += sizeof(int);
    memcpy(fb -> buffer + fb->curr , &de->doc_id,sizeof(int) ); // doc_id
    fb->curr  += sizeof(int);
    memcpy(fb -> buffer + fb->curr , &de->size_of_doc,sizeof(int) ); //  size of this doc
    fb->curr  += sizeof(int);
    memcpy(fb -> buffer + fb->curr , &de->offset,sizeof(long) ); //  size of this doc
    fb->curr  += sizeof(long);
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
    int url_len = total_size - 3*sizeof(int) - sizeof(long);
    fb->curr += sizeof(int);
    memcpy(&de->doc_id, fb->buffer + fb->curr, sizeof(int));
    fb->curr += sizeof(int);

    memcpy(&de->size_of_doc, fb->buffer + fb->curr, sizeof(int));
    fb->curr += sizeof(int);

    memcpy(&de->offset, fb->buffer + fb->curr, sizeof(long));
    fb->curr += sizeof(long);

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


