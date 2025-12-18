#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <zlib.h>

#include "objects.h"
#include "init.h"

int read_object(buf_t *object_contents, char object_name[], const char* oid_name) {
    int obj_fd;

    char d_path[BUFSIZ] = { 0 };
    char dir_name[3];
    oid_dir_str(oid_name, dir_name);
    concat_path(d_path, object_name, dir_name);

    char f_path[BUFSIZ] = { 0 };
    char file_name[60];
    oid_file_str(oid_name, file_name);
    concat_path(f_path, d_path, file_name);

    struct stat stat_buf = { 0 };
    if (stat(f_path, &stat_buf) < 0) {
        fprintf(stderr, "Could not open source file %s (%s)\n", f_path, strerror(errno));
        return -1;
    }
    buf_reserve(object_contents, stat_buf.st_size);

    if ((obj_fd = open(f_path, O_RDONLY)) < 0) {
        return -1;
    }

    // TODO: Create a read_all function
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

int deflate_object(buf_t *src, buf_t *dst) {
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
        strm.avail_out = dst->cap + dst->len;

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
int cat_file(char *oid_name, int pretty) {
    char base[BUFSIZ] = { 0 };

    if (find_base(base, sizeof(base)) == -1) {
        fprintf(stderr, "Could not find repo (%s)\n", strerror(errno));
        return -1;
    }
    char obj_folder[BUFSIZ] = { 0 };
    concat_path(obj_folder, base, "objects");

    if (strlen(oid_name) > 40) {
        // However, searching short hashes is not implemented
        fprintf(stderr, "Invalid argument\n");
        return -1;
    }

    buf_t compressed_object;
    buf_init(&compressed_object);

    if (read_object(&compressed_object, obj_folder, oid_name) < 0) {
        fprintf(stderr, "Object does not exist.\n");
        free_buf(&compressed_object);
        return -1;
    }

    buf_t object_contents;
    buf_init(&object_contents);
    buf_reserve(&object_contents, 64);

    if (deflate_object(&compressed_object, &object_contents) < 0) {
        free_buf(&object_contents);
        free_buf(&compressed_object);
        return -1;
    };
    free_buf(&compressed_object);

    header_t header = { 0 };
    if (parse_header(&object_contents, &header) < 0) {
        free_buf(&object_contents);
        return -1;
    }

    if (read_contents(&object_contents, &header) < 0) {
        free_buf(&object_contents);
        return -1;
    }

    free_buf(&object_contents);
    return 0;
}
