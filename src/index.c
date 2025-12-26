#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <openssl/sha.h>

#include "init.h"
#include "add.h"
#include "index.h"
#include "objects.h"
#include "hash-object.h"

int cmp_path_key_to_entry(const void *key, const void *elem) {
    const char *k = key;
    const index_entry_t *e = elem;
    return strcmp(k, e->path);
}

uint32_t read_be32(const unsigned char *p) {
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |
           ((uint32_t)p[3] <<  0);
}

uint32_t read_be32_seek(const unsigned char *p, size_t *n) {
    const unsigned char *a = p + *n;
    *n += 4;
    return ((uint32_t)a[0] << 24) |
           ((uint32_t)a[1] << 16) |
           ((uint32_t)a[2] <<  8) |
           ((uint32_t)a[3] <<  0);
}

uint16_t read_be16(const unsigned char *p) {
    return ((uint16_t)p[0] << 8) |
           ((uint16_t)p[1] << 0);
}

uint16_t read_be16_seek(const unsigned char *p, size_t *n) {
    const unsigned char *a = p + *n;
    *n += 2;
    return ((uint16_t)a[0] << 8) |
           ((uint16_t)a[1] << 0);
}

void cpy_uint32_seek(unsigned char *buf, uint32_t value, size_t *off) {
    uint32_t bytes = htonl(value);
    memcpy(buf + *off, &bytes, sizeof(uint32_t));
    *off += 4;
}

int write_index_entry(int fd, index_entry_t *entry) {
    unsigned char buf[BUFSIZ] = { 0 };
    size_t offset = 0;
    cpy_uint32_seek(buf, entry->ctime, &offset);
    cpy_uint32_seek(buf, entry->ctime_nanos, &offset);
    cpy_uint32_seek(buf, entry->mtime, &offset);
    cpy_uint32_seek(buf, entry->mtime_nanos, &offset);
    cpy_uint32_seek(buf, entry->dev, &offset);
    cpy_uint32_seek(buf, entry->ino, &offset);
    cpy_uint32_seek(buf, entry->mode, &offset);
    cpy_uint32_seek(buf, entry->uid, &offset);
    cpy_uint32_seek(buf, entry->gid, &offset);
    cpy_uint32_seek(buf, entry->file_size, &offset);
    memcpy(buf + offset, entry->oid.hash, SHA_DIGEST_LENGTH);
    offset += 20;

    uint16_t be_flags = htons(entry->flags);
    memcpy(buf + offset, &be_flags, 2);
    offset += 2;

    // We are ignoring flags for now
    int name_len = entry->flags;
    memcpy(buf + offset, entry->path, name_len);
    offset += name_len;
    buf[offset++] = '\0';

    int padding = (8 - (offset % 8)) % 8;
    for (int i = 0; i < padding; i++) {
        buf[offset++] = '\0';
    }

    if (write_all(fd, buf, offset) < 0) {
        return -1;
    }
    return 0;
}

int write_index_header(int fd, index_state_t *index) {
    unsigned char head[12];
    memcpy(head, "DIRC", 4);

    uint32_t version_be = htonl(index->version);
    uint32_t count_be = htonl(index->entries_count);

    memcpy(head + 4, &version_be, sizeof(uint32_t));
    memcpy(head + 8, &count_be, sizeof(uint32_t));

    return write_all(fd, head, 12);
}

int sha_checksum(int fd, char *tmp_path) {
    SHA_CTX ctx;
    SHA1_Init(&ctx);

    struct stat stat_buf = { 0 };
    if (stat(tmp_path, &stat_buf) < 0) {
        return -1;
    }

    int size = stat_buf.st_size;
    unsigned char buf[size];

    fdatasync(fd);
    if (lseek(fd, 0, SEEK_SET) < 0) return -1;
    int b_read = 0;
    if ((b_read = read_all(fd, buf, size)) < 0 || b_read == 0) {
        return -1;
    }
    if (lseek(fd, 0, SEEK_END) < 0) return -1;

    SHA1_Update(&ctx, buf, size);
    oid_t oid = { 0 };
    SHA1_Final(oid.hash, &ctx);
    if (write_all(fd, oid.hash, 20) < 0) {
        return -1;
    }

    return 0;
}

/// @brief Write the index to file in an atomic way
/// @param path Path to the repository (without /index)
/// @param index Finished index
/// @return -1 on error
int write_index_to_file(char *path, index_state_t *index) {
    char tmp[MAX_PATH];
    concat_path(tmp, MAX_PATH, path, "tmp_XXXXXX");
    int temp_fd = mkstemp(tmp);
    if (temp_fd < 0) {
        close(temp_fd);
        return -1;
    }

    if (write_index_header(temp_fd, index) < 0) {
        close(temp_fd);
        return -1;
    }

    for (uint32_t i = 0; i < index->entries_count; i++) {
        if (write_index_entry(temp_fd, &index->entries[i]) < 0) {
            close(temp_fd);
            return -1;
        }
    }
    if (sha_checksum(temp_fd, tmp) < 0) {
        close(temp_fd);
        return -1;
    }

    char index_path[MAX_PATH];
    concat_path(index_path, MAX_PATH, path, "index");
    rename(tmp, index_path);
    close(temp_fd);
    return 0;
}

// Will check if the entry (path) already exists in the index, and if the HASH is the same
// Sorts the entries again after inserting a new entry
int append_entry(index_state_t *index, index_entry_t *entry) {
    if (entry == NULL || index == NULL) {
        return -1;
    }
    if (index->entries_count > 0) {
        index_entry_t *ret = bsearch(entry->path, index->entries,
                                index->entries_count, sizeof index->entries[0], cmp_path_key_to_entry);
        if (ret != NULL) {
            if (memcmp(ret->oid.hash, entry->oid.hash, SHA_DIGEST_LENGTH) != 0) {
                /// replace existing entry
                free(ret->path);
                *ret = *entry;
                entry->path = NULL;
                return 0;
            } else {
                free(entry->path);
                entry->path = NULL;
                return 0;
            }
        }
    }

    u_int32_t new_count = index->entries_count + 1;

    index_entry_t *new_entries = realloc(index->entries, 
                                    (size_t)new_count * sizeof(index_entry_t));

    if (new_entries == NULL) {
        return -1;
    }

    index->entries = new_entries;
    index->entries[index->entries_count] = *entry;
    index->entries_count = new_count;
    entry->path = NULL;

    sort_index(index);

    return 0;
}

void init_index_state(index_state_t *index) {
    index->version = 2;
    index->entries_count = 0;
    index->entries = NULL;
}

void free_index_state(index_state_t *index) {
    if (index->entries == NULL) {
        return;
    }
    for (uint32_t i = 0; i < index->entries_count; i++) {
        free_entry(&index->entries[i]);
    }
    free(index->entries);
    index->entries = NULL;
    index->entries_count = 0;
    return;
}

void free_entry(index_entry_t *entry) {
    free(entry->path);
    entry->path = NULL;
}

int create_entry(char *path, struct stat *stat_buf, index_state_t *index) {
    index_entry_t idx_e = { 0 };

    idx_e.ctime = (u_int32_t)stat_buf->st_ctim.tv_sec;
    idx_e.ctime_nanos = (u_int32_t)stat_buf->st_ctim.tv_nsec;
    idx_e.mtime = (u_int32_t)stat_buf->st_mtim.tv_sec;
    idx_e.mtime_nanos = (u_int32_t)stat_buf->st_mtim.tv_nsec;
    idx_e.dev = (u_int32_t)stat_buf->st_dev;
    idx_e.ino = (u_int32_t)stat_buf->st_ino;
    idx_e.mode = stat_buf->st_mode;
    idx_e.uid =  stat_buf->st_uid;
    idx_e.gid = stat_buf->st_gid;
    idx_e.file_size = stat_buf->st_size;
    u_int16_t name_len = strlen(path);
    uint16_t n = (name_len >= 0x0FFF) ? 0x0FFF : (uint16_t)name_len;
    idx_e.flags = n;

    char *p = malloc(name_len + 1);
    memcpy(p, path, name_len);
    p[name_len] = '\0';
    idx_e.path = p;

    oid_t oid = { 0 };
    hash_object(&oid, 1, path);
    idx_e.oid = oid;

    if (append_entry(index, &idx_e) < -1) {
        return -1;
    };
    return 0;
}

int read_index_target(char *path, index_state_t *index) {
    struct stat stat_buf;
    if (stat(path, &stat_buf) < 0) {
        errno = ENOENT;
        return -1;
    }

    if (S_ISDIR(stat_buf.st_mode)) {
        DIR *dir = opendir(path);
        if (dir == NULL) {
            errno = ENOENT;
            return -1;
        };
        struct dirent *dr;
        while ((dr = readdir(dir)) != NULL) {
            if (strcmp(dr->d_name, ".") == 0 ||
                strcmp(dr->d_name, "..") == 0) {
                continue;
            }
            char full[MAX_PATH];
            concat_path(full, sizeof(full), path, dr->d_name);
            struct stat st;
            if (stat(full, &st) < 0) {
                errno = ENOENT;
                return -1;
            }
            if (S_ISDIR(stat_buf.st_mode)) {
                if (read_index_target(full, index) < 0) {
                    return -1;
                }
            } else {
                if (create_entry(full, &st, index) < 0) {
                    return -1;
                };
                return 0;
            }
        }
        closedir(dir);
    } else {
        if (create_entry(path, &stat_buf, index) < 0) {
            return -1;
        };
        return 0;
    }
    return 0;
}

int read_entries(buf_t *index, index_state_t *out, int entry_count) {
    size_t n = 12; // bytes processed. Skip header.
    for (int i = 0; i < entry_count; i++) {
        index_entry_t idx_entry = { 0 };
        idx_entry.ctime = read_be32_seek(index->data, &n);
        idx_entry.ctime_nanos = read_be32_seek(index->data, &n);
        idx_entry.mtime = read_be32_seek(index->data, &n);
        idx_entry.mtime_nanos = read_be32_seek(index->data, &n);
        idx_entry.dev = read_be32_seek(index->data, &n);
        idx_entry.ino = read_be32_seek(index->data, &n);
        idx_entry.mode = read_be32_seek(index->data, &n);
        idx_entry.uid = read_be32_seek(index->data, &n);
        idx_entry.gid = read_be32_seek(index->data, &n);
        idx_entry.file_size = read_be32_seek(index->data, &n);

        memcpy(idx_entry.oid.hash, index->data + n, 20);
        n += 20;

        idx_entry.flags = read_be16_seek(index->data, &n);
        u_int16_t name_len = idx_entry.flags & 0x0FFF;
        if ((size_t)(n + name_len) > index->len) {
            errno = EINVAL;
            fprintf(stderr, "Path data exceeds buffer\n");
            return -1;
        }
        char *p = malloc(name_len + 1);
        memcpy(p, index->data + n, name_len);
        p[name_len] = '\0';
        idx_entry.path = p;
        n += name_len;

        while (index->data[n] == '\0') n++;

        if (append_entry(out, &idx_entry) < 0) {
            return -1;
        };
    }
    return 0;
}

int parse_index_header(buf_t *data) {
    if (data->len < 12) {
        fprintf(stderr, "index too small: %zu bytes\n", data->len);
        return -1;
    }

    if (data->data[0] !='D' || data->data[1] !='I'
        || data->data[2] !='R' || data->data[3] !='C') {
            printf("Wrong index signature\n");
        }

    if (read_be32(data->data + 4) != 2) {
        return -1;
    };

    return (int)read_be32(data->data + 8);
}

void read_index(index_state_t *index) {
    printf("read index index->entries_count: %d\n", index->entries_count);
    if (index->entries_count == 0) {
        return;
    };
    for (uint32_t i = 0; i < index->entries_count; i++) {
        printf("%s\n", index->entries[i].path);
    }
    return;
}

int ls_files() {
    char base[BUFSIZ] = { 0 };
    if (find_base(base, sizeof(base)) == -1) {
        fprintf(stderr, "Could not find repo (%s)\n", strerror(errno));
        return -1;
    }

    index_state_t index;
    buf_t idx_contents;
    buf_init(&idx_contents);

    if (init_index(base, &index, &idx_contents) < 0) {
        free_index_state(&index);
        free_buf(&idx_contents);
        return -1;
    }

    read_index(&index);

    free_index_state(&index);
    free_buf(&idx_contents);
    return 0;
}
