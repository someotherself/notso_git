#ifndef INDEX_H
#define INDEX_H

#include <stdint.h>
#include "objects.h"
#

typedef struct {
    uint32_t ctime;
    uint32_t ctime_nanos;
    uint32_t mtime;
    uint32_t mtime_nanos;
    uint32_t dev;
    uint32_t ino;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t file_size;
    oid_t oid;
    uint16_t flags;
    char *path;
} index_entry_t;

typedef struct {
    uint32_t version;
    uint32_t entries_count;
    index_entry_t *entries;
} index_state_t;

int parse_index_header(buf_t *data);
int read_entries(buf_t *index, index_state_t *out, int entry_count);
int read_index_target(char *path, index_state_t *index);
void init_index_state(index_state_t *index);
void free_index_state(index_state_t *index);
void free_entry(index_entry_t *entry);
int write_index_to_file(char *path, index_state_t *index);
int ls_files();

#endif
