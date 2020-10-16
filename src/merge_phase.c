#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "model.h"
#include "termid.h"


/* data structure for one input/output buffer */
typedef struct {file_buffer *fb; term_entry** buf; int curRec; int numRec;} buffer;
typedef struct {int *arr; term_entry** cache; int size; } heapStruct;


#define REC_BUFFER_SIZE 100000

#define KEY(z) (term_id_map[heap.cache[heap.arr[(z)]]->term_id])

buffer *ioBufs;          /* array of structures for in/output buffers */
heapStruct heap;         /* heap structure */
int recSize;             /* size of record (in bytes) */
int recNum;              /* # of records that fit in each buffer */ 

int merge(char N_input, int _maxDegree)
{
    int maxDegree, degree;   /* max allowed merge degree, and actual degree */
    int numFiles = 0;        /* # of output files that are generated */
    term_entry** bufSpace;
    char filename[1024];

    void heapify();
    void writeRecord();
    int nextRecord();
    int i;

    recSize = sizeof(term_entry*);
    recNum = REC_BUFFER_SIZE;
    bufSpace = (term_entry**) malloc(recNum * recSize);
    maxDegree = _maxDegree;
    ioBufs = (buffer *) malloc((maxDegree + 1) * sizeof(buffer));
    heap.arr = (int *) malloc((maxDegree + 1) * sizeof(int));
    heap.cache = (void *) malloc(maxDegree * recSize);
    for( int b = 0; b < N_input; b += maxDegree){
        for (degree = 0; degree < maxDegree && degree < N_input; degree++)
        {
            snprintf(filename, sizeof(filename), "%s-%d", "tmp/sorted", degree);
            file_buffer* fb_in = init_dynamic_buffer(INPUT_BUFFER);
            fb_in -> f = fopen(filename, "r");
            ioBufs[degree].fb = fb_in;
        }
        if (degree == 0) break;

        /* open output file (output is handled by the buffer ioBufs[degree]) */
        sprintf(filename, "%s-%d", "tmp/merged", numFiles);
        file_buffer* fb_o = init_dynamic_buffer(OUTPUT_BUFFER);
        fb_o -> f = fopen(filename, "w");
        ioBufs[degree].fb = fb_o;

        /* assign buffer space (all buffers same space) and init to empty */
        ioBufs[degree].buf = bufSpace;
        ioBufs[degree].curRec = 0;
        ioBufs[degree].numRec = 0;

        /* initialize heap with first elements. Heap root is in heap[1] (not 0) */
        heap.size = degree;
        for (i = 0; i < degree; i++){
            heap.arr[i+1] = nextRecord(i);
            if(heap.arr[i+1] == -1)
                heap.size--;
        }
        for (i = degree; i > 0; i--)  heapify(i);

        /* now do the merge - ridiculously simple: do 2 steps until heap empty */
        while (heap.size > 0)
        {
            /* copy the record corresponding to the minimum to the output */
            writeRecord(&(ioBufs[degree]), heap.arr[1]); 

            /* replace minimum in heap by the next record from that file */
            if (nextRecord(heap.arr[1]) == -1)
                heap.arr[1] = heap.arr[heap.size--];     /* if EOF, shrink heap by 1 */
            if (heap.size > 1)  heapify(1);
        }
        
        /* flush output, add output file to list, close in/output files, and next */
        writeRecord(&(ioBufs[degree]), -1); 
        for (i = 0; i <= degree; i++) {
            fclose(ioBufs[i].fb->f);
            free_file_buffer(ioBufs[i].fb);
        }
        printf("Successfully merged to file %s\n",filename);
        numFiles++;

        for (degree = 0; degree < maxDegree && degree < N_input; degree++)
        {
            snprintf(filename, sizeof(filename), "%s-%d", "tmp/sorted", degree);
            remove(filename);
        }
    }

    free(bufSpace);
    free(ioBufs);
    free(heap.arr);
    free(heap.cache);
    return 1;
}


/* standard heapify on node i. Note that minimum is node 1. */
void heapify(int i){ 
  int s, t;

  s = i;
  while(1)
  {
    /* find minimum key value of current node and its two children */

    if (((i<<1) <= heap.size) && strcmp(KEY(i<<1) , KEY(i)))  s = i<<1;
    if (((i<<1)+1 <= heap.size) && strcmp(KEY((i<<1)+1) , KEY(s)))  s = (i<<1)+1;
    
    /* if current is minimum, then done. Else swap with child and go down */
    if (s == i)  break;
    t = heap.arr[i];
    heap.arr[i] = heap.arr[s];
    heap.arr[s] = t;
    i = s;
  }
}


/* get next record from input file into heap cache; return -1 if EOF */
int nextRecord(int i){
    buffer *b = &(ioBufs[i]);

    term_entry* te = (term_entry*)malloc(sizeof(term_entry));
    te->chunk_list_head = NULL;
    if(next_record_from_file_buffer(b->fb, te) == false){
        _free_te(te);
        return -1;
    }
    heap.cache[i] = te;
    return(i);
}


/* copy i-th record from heap cache to out buffer; write to disk if full */
/* If i==-1, then out buffer is just flushed to disk (must be last call) */
void writeRecord(buffer *b, int i)
{
    int j;
    if( i != -1 && b->curRec >= 1 && b->buf[b->curRec - 1]->term_id == heap.cache[i]->term_id){
        merge_same_term(b->buf[b->curRec - 1], heap.cache[i]);
        _free_te(heap.cache[i]);
        return;
    }
        
    /* flush buffer if needed */
    if ((i == -1) || (b->curRec == recNum))
    { 
        bool _flush = (i==-1)?true:false;
        for (j = 0; j < b->curRec; j++){
            write_record(b->fb, b->buf[j],_flush);
            _free_te(b->buf[j]);
        }
        b->curRec = 0;
    }

    if (i != -1){
        b->buf[(b->curRec++)] = heap.cache[i];
            // memcpy(&(b->buf[(b->curRec++)*recSize]), heap.cache+i*recSize, recSize);
    }
}
