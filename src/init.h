#ifndef INIT_H
#define INIT_H

#include <string.h>

void init();

// Finds the base directory ".notso_git" by searcing cwd then walking up the path to root
// If base directory is found, it returns the directory with the base dir added.
int find_base(char *out, size_t outsize);

#endif
