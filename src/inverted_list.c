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
#include "inverted_list.h"
#include "model.h"
#include "var_bytes.h"

IV* openList(int offset, int maxdocID){
    IV* iv = (IV*) malloc(sizeof(IV));
    // iv->f = fopen("output/inverted_list", "r");
    iv->f = fopen("output/inverted_list", "r");

    fseek(iv->f, offset, SEEK_SET);
    iv->length = vb_decode_stream(iv->f);
    int block_lastdoc_table_size = vb_decode_stream(iv->f);
    int block_size_table_size = vb_decode_stream(iv->f);
    iv->block_ldoc_table_offset = ftell(iv->f);
    iv->block_size_table_offset = iv->block_ldoc_table_offset + block_lastdoc_table_size;
    iv->block_offset = iv->block_size_table_offset + block_size_table_size;
    iv->block_cache = NULL;
    iv->block_cache_ptr = NULL;
    iv->block_curr_ldoc = 0;
    iv->block_curr_sdoc = 0;
    iv->block_curr_cnt = 0;
    iv->maxdocID = maxdocID;
    return iv;
}

int nextGEQ(IV* lp, int k){
    if (lp->block_curr_ldoc >= k && lp->block_cache_ptr != NULL){
        while(lp->block_cache_ptr != NULL){
            if(lp->block_cache_ptr -> doc_id >= k){
                int r = lp -> block_cache_ptr -> doc_id;
                int f = lp -> block_cache_ptr -> freq;
                lp->block_cache_ptr = lp->block_cache_ptr->next;
                lp->curr_doc = r;
                lp->curr_freq = f;
                return r;
            }
            lp -> block_cache_ptr = lp->block_cache_ptr->next;
        }
    }
    fseek(lp->f,lp -> block_ldoc_table_offset, SEEK_SET);
    while(lp -> block_ldoc_table_offset < lp -> block_size_table_offset){
        int ldoc = vb_decode_stream(lp->f);
        lp -> block_curr_sdoc = lp->block_curr_ldoc;
        lp -> block_curr_ldoc = ldoc;
        lp -> block_curr_cnt += 1;
        lp -> block_ldoc_table_offset = ftell(lp->f);
        if(ldoc >= k){
            free_cache(lp);
            lp->block_cache = read_block_to_cache(lp);
            lp->block_cache_ptr = lp->block_cache;
            fseek(lp->f,lp -> block_ldoc_table_offset,SEEK_SET);
            while(lp->block_cache_ptr != NULL){
                if(lp->block_cache_ptr -> doc_id >= k){
                    int r = lp->block_cache_ptr -> doc_id;
                    int f = lp->block_cache_ptr -> freq;
                    lp->block_cache_ptr = lp->block_cache_ptr->next;
                    lp->curr_doc = r;
                    lp->curr_freq = f;
                    return r;
                }
                lp->block_cache_ptr = lp->block_cache_ptr->next;
            }
        }
    }
    lp->curr_doc = lp->maxdocID;
    return lp->maxdocID;
}

void free_cache(IV* iv){
    term_chunk* ptr = iv->block_cache;
    while(ptr){
        term_chunk* tmp = ptr->next;
        free(ptr);
        ptr = tmp;
    }
}

int getFreq(IV* iv){
    return iv->curr_freq;
}

void closeList(IV* iv){
    fclose(iv->f);
    free_cache(iv);
}