#ifndef ADD_H
#define ADD_H

#include "index.h"

int add(char *path);
void sort_index(index_state_t *index);
int init_index(char *base, index_state_t *index, buf_t *idx_contents);

#endif