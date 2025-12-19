#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "objects.h"
#include "init.h"

int cat_file(char *oid_name, int pretty) {
    char base[BUFSIZ] = { 0 };

    if (find_base(base, sizeof(base)) == -1) {
        fprintf(stderr, "Could not find repo (%s)\n", strerror(errno));
        return -1;
    }
    char obj_folder[BUFSIZ] = { 0 };
    concat_path(obj_folder, base, "objects");

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

    if (decompress_object(&compressed_object, &object_contents) < 0) {
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
