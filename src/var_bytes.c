
#include <stdlib.h>
#include <stdio.h>

unsigned char* vb_encode(int n, int* L ){
    int m = n;
    int d = 0;
    if( m == 0 )
        d = 1;
    while(m != 0){ m/=128; d++;}
    unsigned char* bytes = (unsigned char*)malloc(d);
    for(int i = 0; i < d; i++, n /= 128)
        bytes[i] = n % 128; 

    bytes[d-1] += 128;
    *L = d;
    return bytes;
}

int vb_decode(unsigned char* bytes){
    int i = 0,r = 0,n = 0, k = 1;
    do{
        n = bytes[i] % 128;
        r = n*k + r;
        k = k*128;
    }while(bytes[i++] < 128);
    return r;
}

int vb_decode_stream(FILE* f){
    int r = 0,n = 0, k = 1;
    unsigned char c = 0;
    do{
        fread(&c, 1, 1, f);
        n = c % 128;
        r = n*k + r;
        k = k*128;
    }while(c < 128 && !feof(f));
    return r;
}
