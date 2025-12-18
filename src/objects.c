#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "objects.h"

// ----------------------------
// dyn array
// ----------------------------

void buf_init(buf_t *b) {
    b->data = NULL;
    b->len = b->cap = 0;
}

void free_buf(buf_t *b) {
    free(b->data);
}

void buf_reserve(buf_t *b, size_t s) {
    if (b->cap < s) {
        int new_cap = s;
        void *p = realloc(b->data, new_cap);
        b->data = p;
        b->cap = new_cap;
    }
}

int buf_append(buf_t *b, const void *data, size_t s) {
    // Check if we have enough capacity
    if (b->len + s > b->cap) {
        size_t new_cap = b->cap ? b->cap * 2 : 64;
        while (new_cap < b->len + s) new_cap *=2;

        void *p = realloc(b->data, new_cap);
        if (p == NULL) return -1;
        b->data = p;
        b->cap = new_cap;
    }
    // Insert the new element
    memcpy(b->data + b->len, data, s);
    b->len += s;
    return 0;
}

int buf_grow(buf_t *b, size_t extra) {
    size_t needed = b->len + extra;
    if (b->cap >= needed) return 0; // We have enough cap

    size_t new_cap = b->cap ? b->cap * 2 : 64;
    while (new_cap < needed) new_cap *= 2;

    void *p = realloc(b->data, new_cap);
    if (p == NULL) return -1;
    b->data = p;
    b->cap = new_cap;
    return 0;
}

// ----------------------------
// Path helpers
// ----------------------------

/* writes 2 hex chars + '\0' */
void oid_dir(const Oid *oid, char out[3]) {
    snprintf(out, 3, "%02x", oid->hash[0]);
}

/* writes 38 hex chars + '\0' */
void oid_file(const Oid *oid, char out[39]) {
    for (int i = 1; i < SHA_DIGEST_LENGTH; i++) {
        sprintf(out + (i - 1) * 2, "%02x", oid->hash[i]);
    }
}

/* writes 2 string chars + '\0' */
void oid_dir_str(const char *oid_hex, char out[3]) {
    memcpy(out, oid_hex, 2);
    out[2] = '\0';
}

/* writes 38 string chars + '\0' */
void oid_file_str(const char *oid_hex, char out[39]) {
    memcpy(out, oid_hex + 2, 38);
    out[38] = '\0';
}

// Concatenates 2 path components to create a valid path
// Handles cases where both ends contain '/', only one, or neither.
// Paths must be valid
// TODO: Add error checking and use static p1 and p2
void concat_path(char *out, char* p1, char* p2) {
    
    if (p1[strlen(p1) - 1] == 47 && p2[0] == 47) {
        p1[strlen(p1) - 1] = 0;
    }
    if (p1[strlen(p1) - 1] == 47 || p2[0] == 47) {
        snprintf(out, MAX_PATH, "%s%s", p1, p2);
        return;
    }
    snprintf(out, MAX_PATH, "%s/%s", p1, p2);
}

// ----------------------------
// Object Header
// ----------------------------

const struct str_to_obj_t head_conversion[] = {
    {OBJ_BLOB, "blob"},
    {OBJ_TREE, "tree"},
    {OBJ_COMMIT, "commit"},
    {OBJ_UNKNOWN, ""},
};

obj_type_t obj_t_from_str(const char *str) {
    int i;
    for (i = 0; i < (int)(sizeof(head_conversion) / sizeof(head_conversion[0])); i++) {
        if (strcmp(str, head_conversion[i].str) == 0) {
            return head_conversion[i].type;
        }
    }
    return OBJ_UNKNOWN;
}

int parse_header(buf_t *src, header_t *header) {
    if (!src || !header || !src->data) return -1;

    // end of the header
    unsigned char *nul = memchr(src->data, '\0', src->len);
    if (nul == NULL) return -1;

    size_t header_len = (size_t)(nul - src->data) + 1;

    // end of object type
    unsigned char *space = memchr(src->data, ' ', header_len);
    if (space == NULL) return -1;

    size_t type_len = (size_t)(space - src->data);

    char type_buf[40];
    memcpy(type_buf, src->data, type_len);
    type_buf[type_len] = '\0';
    header->obj_type = obj_t_from_str(type_buf);
    if (header->obj_type == OBJ_UNKNOWN) {
        fprintf(stderr, "Unknown object: %s\n", type_buf);
        return -1;
    }

    const char *size_str = (const char*)(space + 1);
    errno = 0;
    char *end = NULL;
    unsigned long long size = strtoull(size_str, &end, 10);
    if (errno != 0) return -1;
    if ((unsigned char*)end != nul) return -1;

    header->size = (size_t)size;
    return 0;
}

// ----------------------------
// Object payload
// ----------------------------

int read_contents(buf_t *src, header_t *header) {
    if (header->obj_type == OBJ_BLOB) {
        unsigned char *nul = memchr(src->data, '\0', src->len);
        size_t header_len = (size_t)(nul - src->data + 1);
        if (nul == NULL) return -1;
        char content_buf[header->size + 1];
        memcpy(content_buf, src->data + header_len, header->size);
        printf("%s", content_buf);
    }

    return 0;
}