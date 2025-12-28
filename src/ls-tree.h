#ifndef LS_TREE_H
#define LS_TREE_H

#include "objects.h"

int ls_tree(char *oid_name);
int read_tree(unsigned char *contents, size_t content_len);

#endif