#ifndef OBJECT_H
#define OBJECT_H

#include <stddef.h>
#include <stdint.h>
#include <openssl/sha.h>

#define HASH_HEX_LEN 40
#define MAX_PATH     4096

typedef struct {
    unsigned char hash[SHA_DIGEST_LENGTH];
} Oid;

void oid_dir(const Oid *oid, char out[3]);
void oid_file(const Oid *oid, char out[39]);
void oid_dir_str(const char *oid_hex, char out[3]);
void oid_file_str(const char *oid_hex, char out[39]);

typedef enum {
    OBJ_BLOB = 0,
    OBJ_TREE,
    OBJ_COMMIT,
    OBJ_UNKNOWN,
} obj_type_t;

struct str_to_obj_t {
    obj_type_t type;
    const char *str;
};

extern const struct str_to_obj_t head_conversion[];

// https://stackoverflow.com/questions/16844728/converting-from-string-to-enum-in-c
obj_type_t obj_t_from_str(const char *str);

typedef struct {
    unsigned char *data;
    size_t len;
    size_t cap;
} buf_t;

void buf_init(buf_t *b);
void free_buf(buf_t *b);
void buf_reserve(buf_t *b, size_t s);
int buf_append(buf_t *b, const void *data, size_t s);
int buf_grow(buf_t *b, size_t extra);

void concat_path(char *out, char* p1, char* p2);

typedef struct {
    obj_type_t obj_type;
    size_t size;
} header_t;

int parse_header(buf_t *src, header_t *header);
int read_contents(buf_t *src, header_t *header);

#endif
