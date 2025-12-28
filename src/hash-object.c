#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <zlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <openssl/sha.h>

#include "init.h"
#include "objects.h"

/// @brief Creates a blob in memory.
/// @param oid Output. Pointer to a `oid_t` struct that will containt the object SHA1 HASH.
/// @param fd File descriptor to the file file converted to blob.
/// @param obj Output. Pointer to a `buf_t` struct containing the in memory blob.
/// @param size Size of the file converted to blob.
/// @return -1 on error.
int create_blob(oid_t *oid, int fd, buf_t *obj, size_t size) {
    char header[64];

    size_t header_len = snprintf(header, sizeof(header), "blob %zu", size) + 1;

    if (header_len <= 0 || header_len > sizeof(header)) {
        return -1;
    }

    if (buf_append(obj, header, header_len) < 0) {
        return -1;
    }

    size_t off = obj->len; // content already existing in the array

    buf_reserve(obj, size + header_len);
    ssize_t n = read_all(fd, obj->data + off, size);
    if (n < 0 || (size_t)n != size) {
        errno = EIO;
        return -1;
    }
    obj->len += n;

    // Create the SHA1
    SHA_CTX ctx;
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, obj->data, obj->len);
    SHA1_Final(oid->hash, &ctx);

    return 0;
}

/// @brief Creates a empty file where a git object will be saved.
/// @param oid Pointer to a `oid_t` struct that containing the object SHA1 HASH.
/// @param repo Path to the .notso_git folder
/// @return -1 on error
int create_target(oid_t *oid, char *repo) {
    int fd;

    char o_path[BUFSIZ];
    if (concat_path(o_path, sizeof(o_path), repo, "objects") < 0) {
        fprintf(stderr, "Path too long: repo/objects\n");
        return -1;
    };

    char d_path[BUFSIZ];
    char dir_name[10];
    oid_dir(oid, dir_name);
    if (concat_path(d_path, sizeof(d_path), o_path, dir_name) < 0) {
        fprintf(stderr, "Path too long: repo/objects/HASH\n");
        return -1;
    };
    if (mkdir(d_path, 0755) < 0) {
        if (errno != EEXIST) {
            return -1;
        }
    }

    char f_path[BUFSIZ];
    char file_name[60];
    oid_file(oid, file_name);
    if (concat_path(f_path, sizeof(f_path), d_path, file_name) < 0) {
        fprintf(stderr, "Path too long: repo/objects/HASH/HASH\n");
        return -1;
    };
    if (access(f_path, F_OK) == 0) {
        return 0;
    } 
    fd = open(f_path, O_WRONLY | O_TRUNC | O_CREAT, 0644) ;
    if (fd <= 0) {
        return -1;
    };

    return fd;
}

/// @brief Main function handling the logic behind hash-object
/// @param oid Output. Pointer to a `oid_t` struct that will containt the object SHA1 HASH.
/// @param fd File descriptor to the file file converted to blob.
/// @param stat_buf `stat` struct for the file, obtained from stat()
/// @param repo Path to the .notso_git folder
/// @param write_arg bool for the -w argument
/// @return -1 on error
int hash_file(oid_t *oid, int fd, struct stat *stat_buf, char *repo, int write_arg) {
    int oid_fd; // fd of the blob
    buf_t oid_content;
    buf_init(&oid_content);

    if (create_blob(oid, fd, &oid_content, stat_buf->st_size) < 0) {
        free_buf(&oid_content);
        fprintf(stderr, "Could not create HASH (%s)\n", strerror(errno));
        return -1;
    }

    if (!write_arg) {
        // Early exit if not writing blob to disk
        free_buf(&oid_content);
        return 0;
    }

    // Compress it
    uLong src_len = oid_content.len;
    uLong dst_len = compressBound(src_len);

    buf_t compressed;
    buf_init(&compressed);
    buf_reserve(&compressed, dst_len);

    int ret = compress((Bytef *)compressed.data, &dst_len, (Bytef *)oid_content.data, src_len);

    if (ret != 0) {
        // Early return
        free_buf(&oid_content);
        free_buf(&compressed);
        fprintf(stderr, "Could compress blob (%s)\n", strerror(errno));
        return -1;  
    }
    compressed.len = dst_len;

    // Create and open a blob file
    if ((oid_fd = create_target(oid, repo)) < 0) { 
        fprintf(stderr, "Could not create blob target (%s)\n", strerror(errno));
        free_buf(&oid_content);
        free_buf(&compressed);
        return -1;
    }
    if (oid_fd == 0) {
        // Blob already exists
        free_buf(&oid_content);
        free_buf(&compressed);
        return 0;
    }

    // ssize_t n = write_all(oid_fd, compressed.data, compressed.len);
    int n = write(oid_fd, compressed.data, compressed.len);
    if (n < 0 || (size_t)n != compressed.len) {
        // Early return
        fprintf(stderr, "Failed to write to blob (%s)\n", strerror(errno));
        close(oid_fd);
        free_buf(&oid_content);
        free_buf(&compressed);
        return -1;
    }

    close(oid_fd);
    free_buf(&oid_content);
    free_buf(&compressed);
    return 0;
}

int hash_object(oid_t *oid, int write, char *file) {
    char base[BUFSIZ] = { 0 };
    if (find_base(base, sizeof(base)) == -1) {
        fprintf(stderr, "Could not find repo (%s)\n", strerror(errno));
        return -1;
    }

    int fd; // Blob will be saved here

    if ((fd = open(file, O_RDONLY)) < 0) {
        fprintf(stderr, "Could not open source file %s (%s)\n", file, strerror(errno));
        return -1;
    }

    struct stat stat_buf;
    if (fstat(fd, &stat_buf) < 0) {
        fprintf(stderr, "Could not check stats %s (%s)\n", file, strerror(errno));
        close(fd);
        return -1;
    }

    if (hash_file(oid, fd, &stat_buf, base, write) < 0) {
        fprintf(stderr, "Could not hash file %s (%s)\n", file, strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

