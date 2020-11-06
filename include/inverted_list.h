#ifndef IV_H
#define IV_H

#include "model.h"

IV* openList(int offset, int maxdocID);
int nextGEQ(IV* lp,int k);
int getFreq(IV* lp);
void closeList(IV* iv);
void free_cache(IV* iv);

#endif