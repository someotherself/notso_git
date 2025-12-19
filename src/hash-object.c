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

int create_blob(Oid *oid, int fd, buf_t *obj, size_t size) {
    char header[64];

    int header_len = snprintf(header, sizeof(header), "blob %zu", size) + 1;

    if (header_len <= 0 || (size_t)header_len > sizeof(header)) {
        return -1;
    }

    if (buf_append(obj, header, (size_t)header_len) < 0) {
        printf("Buf append error\n");
        return -1;
    }

    unsigned char tmp[BUFSIZ];
    for (;;) {
        ssize_t n = read(fd, tmp, sizeof(tmp));
        if (n < 0) return -1;
        if (n == 0) break;
        // Write the bytes to the object
        if (buf_append(obj, tmp, (size_t)n) < 0) return -1;
    }

    // Create the SHA1
    SHA_CTX ctx;
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, obj->data, obj->len);
    SHA1_Final(oid->hash, &ctx);

    return 0;
}

int create_target(Oid *oid, char *repo) {
    int fd;

    char o_path[BUFSIZ];
    concat_path(o_path, repo, "objects");

    char d_path[BUFSIZ];
    char dir_name[10];
    oid_dir(oid, dir_name);
    concat_path(d_path, o_path, dir_name);
    if (mkdir(d_path, 0755) < 0) {
        if (errno != EEXIST) {
            return -1;
        }
    }

    char f_path[BUFSIZ];
    char file_name[60];
    oid_file(oid, file_name);
    concat_path(f_path, d_path, file_name);
    if (access(f_path, F_OK) == 0) {
        return 0;
    } 
    fd = open(f_path, O_WRONLY | O_TRUNC | O_CREAT, 0644) ;
    if (fd <= 0) {
        return -1;
    };

    return fd;
}

int hash_file(Oid *oid, int fd, struct stat *stat_buf, char *repo, int write_arg) {
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
        return -1;
    }
    if (oid_fd == 0) {
        // Blob already exists
        return 0;
    }

    // TODO: Create a write_all function
    int n = write(oid_fd, compressed.data, compressed.len);
    if (n < 0) {
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

int hash_object(int write, char *file) {
    char base[BUFSIZ] = { 0 };
    if (find_base(base, sizeof(base)) == -1) {
        fprintf(stderr, "Could not find repo %s (%s)\n", file, strerror(errno));
        return -1;
    }

    int fd; // Blob will be saved here
    Oid oid = { 0 }; // Holds the blob hash

    if ((fd = open(file, O_RDONLY)) < 0) {
        fprintf(stderr, "Could not open source file %s (%s)\n", file, strerror(errno));
        return -1;
    }

    struct stat stat_buf;
    if (stat(file, &stat_buf) < 0) {
        fprintf(stderr, "Could not check stats %s (%s)\n", file, strerror(errno));
        close(fd);
        return -1;
    }

    if (hash_file(&oid, fd, &stat_buf, base, write) < 0) {
        fprintf(stderr, "Could not hash file %s (%s)\n", file, strerror(errno));
        close(fd);
        return -1;
    }

    for (int i = 0; i < 20; i++) {
        printf("%02x", oid.hash[i]);
    }
    printf("\n");

    close(fd);
    return 0;
}
