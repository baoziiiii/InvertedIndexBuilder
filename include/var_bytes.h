#ifndef VBENCODE_H
#define VBENCODE_H

#include <stdio.h>

unsigned char* vb_encode(int n, int* L );
int vb_decode(unsigned char* bytes);
int vb_decode_stream(FILE* f);

#endif
