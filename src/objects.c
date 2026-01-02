#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <zlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include "objects.h"
#include "ls-tree.h"

// ----------------------------
// dyn array
// ----------------------------

void buf_init(buf_t *b) {
    b->data = NULL;
    b->len = b->cap = 0;
}

void free_buf(buf_t *b) {
    free(b->data);
    b->data = NULL;
}

/// @brief Reserves capacity in the array. Does not update the length field.
/// @param b Pointer to a buf_t byte array
/// @param s Total capacity needed. The value is not added to the current capacity.
void buf_reserve(buf_t *b, size_t s) {
    if (b->cap < s) {
        size_t new_cap = s;
        void *p = realloc(b->data, new_cap);
        b->data = p;
        b->cap = new_cap;
    }
}

/// @brief Adds bytes to the array. Will update the length field.
/// @param b Pointer to a buf_t byte array
/// @param data Data to be added to the array.
/// @param s Size of the data
/// @return -1 on error
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

/// @brief Adds extra capacity to the array. Does not update the length field.
/// @param b Pointer to a buf_t byte array
/// @param extra Capacity needed, added in adition to the existing capacity.
/// @return -1 on error
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
// hex
// ----------------------------

// Convert 20 bytes binary sha-1 to a 40 char hex string
void oid_to_hex(const unsigned char oid[20], char hex[41]) {
    static const char *digits = "0123456789abcdef";
    for (int i = 0; i < 20; i++) {
        hex[i*2 + 0] = digits[(oid[i] >> 4) & 0xF];
        hex[i*2 + 1] = digits[oid[i] & 0xF];
    }
    hex[40] = '\0';
}

/// @brief Helper to print SHA1 hash
/// @param oid Pointer to a oid_t struct
void print_hex(oid_t *oid) {
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        printf("%02x", (unsigned)oid->hash[i]);
    }
    putchar('\n');
}

// ----------------------------
// Path helpers
// ----------------------------

/// @brief Helper to create the folder for a git object. Writes 2 hex chars + '\0'
/// @param oid Pointer to a oid_t struct
/// @param out Output containing the directory name + null terminator.
void oid_dir(const oid_t *oid, char out[3]) {
    snprintf(out, 3, "%02x", oid->hash[0]);
}

/// @brief Helper to create the file for a git object. Writes 38 hex chars + '\0'
/// @param oid Pointer to a oid_t struct
/// @param out Output containing the file name + null terminator.
void oid_file(const oid_t *oid, char out[39]) {
    for (int i = 1; i < SHA_DIGEST_LENGTH; i++) {
        sprintf(out + (i - 1) * 2, "%02x", oid->hash[i]);
    }
}

/// @brief Helper to create the folder for a git object. Writes 2 STRING chars + '\0'
/// @param oid Pointer to a oid_t struct
/// @param out Output containing the directory name + null terminator.
void oid_dir_str(const char *oid_hex, char out[3]) {
    memcpy(out, oid_hex, 2);
    out[2] = '\0';
}

/// @brief Helper to create the file for a git object. Writes 38 STRING chars + '\0'
/// @param oid Pointer to a oid_t struct
/// @param out Output containing the file name + null terminator.
void oid_file_str(const char *oid_hex, char out[39]) {
    memcpy(out, oid_hex + 2, 38);
    out[38] = '\0';
}

// Concatenates 2 path components
int concat_path(char *out, size_t out_size, const char* p1, const char* p2) {
    int n = snprintf(out, out_size, "%s/%s", p1, p2);
    return (n >= 0 && (size_t)n < out_size) ? 0 : -1;
}

// ----------------------------
// Object Header
// ----------------------------

/// @brief Helper to convert a string to an object type
const struct str_to_obj_t head_conversion[] = {
    {OBJ_BLOB, "blob"},
    {OBJ_TREE, "tree"},
    {OBJ_COMMIT, "commit"},
    {OBJ_UNKNOWN, ""},
};

/// @brief Helper to convert a string to an object type
obj_type_t obj_t_from_str(const char *str) {
    int i;
    for (i = 0; i < (int)(sizeof(head_conversion) / sizeof(head_conversion[0])); i++) {
        if (strcmp(str, head_conversion[i].str) == 0) {
            return head_conversion[i].type;
        }
    }
    return OBJ_UNKNOWN;
}

/// @brief Helper to convert a mode to an object type
const struct mode_to_obj_t mode_conversion[] = {
    {OBJ_BLOB, "100644"},
    {OBJ_TREE, "40000"},
    {OBJ_COMMIT, "160000"},
    {OBJ_UNKNOWN, ""},
};

/// @brief Helper to convert a mode to an object type
obj_type_t obj_t_from_mode(const char *mode) {
    int i;
    for (i = 0; i< (int)(sizeof(mode_conversion) / sizeof(mode_conversion[0])); i++) {
        if (strcmp(mode, mode_conversion[i].mode) == 0) {
            return mode_conversion[i].type;
        }
    };
    return OBJ_UNKNOWN;
}

/// @brief Helper to convert a mode to an object name as string
const struct mode_to_str mode_str_conversion[] = {
    {"blob", "100644"},
    {"blob", "120000"},
    {"blob", "100755"},
    {"tree", "40000"},
    {"commit", "160000"},
};

/// @brief Helper to convert a mode to an object name as string
char* mode_to_str(const char *mode) {
    for (size_t i = 0; i < (sizeof(mode_str_conversion) / sizeof(mode_str_conversion[0])); i++) {
        if (strcmp(mode, mode_str_conversion[i].mode) == 0) {
            return mode_str_conversion[i].str;
        }
    }
    return "unknown";
}

// Reads the header of a blob, tree or commit to get object type and payload size
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

/// Finds the objects (blob, tree or commit) and reads the (compressed) contents into object_contents
int read_object(buf_t *object_contents, char object_folder[], const char* oid_name) {
    int obj_fd;

    char d_path[BUFSIZ] = { 0 };
    char dir_name[3];
    oid_dir_str(oid_name, dir_name);
    if (concat_path(d_path, sizeof(d_path), object_folder, dir_name) < 0) {
        fprintf(stderr, "Path too long: repo/objects/HASH\n");
        return -1;
    };

    char f_path[BUFSIZ] = { 0 };
    char file_name[60];
    oid_file_str(oid_name, file_name);
    if (concat_path(f_path, sizeof(f_path), d_path, file_name) < 0) {
        fprintf(stderr, "Path too long: repo/objects/HASH/HASH\n");
        return -1;
    };

    if ((obj_fd = open(f_path, O_RDONLY)) < 0) {
        return -1;
    }

    struct stat stat_buf = { 0 };
    if (fstat(obj_fd, &stat_buf) < 0) {
        fprintf(stderr, "Could not open source file %s (%s)\n", f_path, strerror(errno));
        return -1;
    }
    buf_reserve(object_contents, stat_buf.st_size);

    int b_read;
    if ((b_read = read(obj_fd, object_contents->data, object_contents->cap)) < 0) {
        fprintf(stderr, "Failed to read object to blob (%s)\n", strerror(errno));
        close(obj_fd);
        return -1;
    }
    object_contents->len = b_read;

    close(obj_fd);
    return 0;
}

/// @brief Decompresses a git object using zlib
/// @param src A pointer to a buf_t array containing the compressed object
/// @param dst A pointer to a buf_t array where the object will be decompressed.
/// @return -1 on error.
int decompress_object(buf_t *src, buf_t *dst) {
    z_stream strm = {0};

    if (inflateInit(&strm) != Z_OK) {
        return -1;
    }

    strm.next_in = src->data;
    strm.avail_in = src->len;

    int ret;
    do {
        if (buf_grow(dst, 1024) < 0) {
            inflateEnd(&strm);
            return -1;
        }

        strm.next_out = dst->data + dst->len;
        strm.avail_out = dst->cap - dst->len;

        ret = inflate(&strm, Z_NO_FLUSH);

        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&strm);
            return -1;
        }

        dst->len = dst->cap - strm.avail_out;

    } while (ret != Z_STREAM_END);
    inflateEnd(&strm);
    return 0;
}

/// @brief Used by cat-file to read the contents of a git object.
/// @param src Pointer to a `buf_t` array, containing the decompressed contents of the object.
/// @param header Pointer to a `header_t` struct containing the object header
/// @return -1 on error
int read_contents(buf_t *src, header_t *header) {
    unsigned char *nul = memchr(src->data, '\0', src->len);
    if (nul == NULL) return -1;
    size_t header_len = (size_t)(nul - src->data + 1);
    size_t data_size = header->size + 1;
    buf_t contents = { 0 };
    buf_init(&contents);
    buf_reserve(&contents, data_size);
    // char content_buf[header->size + 1];

    if (header->obj_type == OBJ_BLOB) {
        memcpy(contents.data, src->data + header_len, header->size);
        contents.len += data_size;
        contents.data[header->size] = '\0'; // Null terminator for printing
        printf("%s", contents.data);
    }

    if (header->obj_type == OBJ_TREE) {
        memcpy(contents.data, src->data + header_len, header->size);
        contents.len += data_size;
        read_tree(contents.data, header->size);
    }

    if (header->obj_type == OBJ_COMMIT) {
        printf("Not implemented for commits.\n");
    }

    free_buf(&contents);
    return 0;
}

// ----------------------------
// fs functions
// ----------------------------

ssize_t read_all(const int fd, unsigned char *buf, size_t size) {
    size_t b_read = 0;
    for (;;) {
        ssize_t n = read(fd, buf + b_read, size - b_read);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        };
        if (n == 0) break;
        b_read += n;
    }
    return b_read;
}

ssize_t write_all(const int fd, unsigned char *buf, size_t size) {
    size_t b_written = 0;
    while (b_written < size) {
        ssize_t n = write(fd, buf + b_written, size - b_written);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        };
        if (n == 0) {
            errno = EIO;
            return -1;
        };
        b_written += n;
    }
    return b_written;
}
