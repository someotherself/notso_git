#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "objects.h"
#include "init.h"

/// @brief Contains the main logic behind the cat-file commant.
/// @param oid_name Input. Object SHA1 hash passed in by the user.
/// @param pretty Not used.
/// @return -1 on error
int cat_file(char *oid_name, int pretty) {
    char base[BUFSIZ] = { 0 };
    UNUSED(pretty);

    if (find_base(base, sizeof(base)) == -1) {
        fprintf(stderr, "Could not find repo (%s)\n", strerror(errno));
        return -1;
    }
    char obj_folder[BUFSIZ] = { 0 };
    if (concat_path(obj_folder, sizeof(obj_folder), base, "objects") < 0) {
        fprintf(stderr, "Path too long: repo/objects\n");
        return -1;
    };

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
