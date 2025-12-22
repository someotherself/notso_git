#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "objects.h"
#include "init.h"

int add(char *path) {
    printf("Received path: %s\n", path);
    char base[BUFSIZ] = { 0 };
    if (find_base(base, sizeof(base)) == -1) {
        fprintf(stderr, "Could not find repo (%s)\n", strerror(errno));
        return -1;
    }
    // Check if an index already exists and read it in memory
    char index[BUFSIZ] = { 0 };
    if (concat_path(index, sizeof(index), base, "index") < 0) {
        fprintf(stderr, "Path too long: repo/index\n");
        return -1;
    }

    int idx_exists = 0;
    struct stat stat_buf;
    buf_t idx_contents;
    buf_init(&idx_contents);
    buf_reserve(&idx_contents, 4);
    if (stat(index, &stat_buf) == 0) {
        int index_fd = open(index, O_RDONLY);
        if (index_fd >= 0) {
            idx_exists = 1;
            buf_reserve(&idx_contents, stat_buf.st_size);
            int b_read = read(index_fd, &idx_contents.data, stat_buf.st_size);
            idx_contents.len = b_read;
            close(index_fd);
        }
    }

    UNUSED(idx_exists);
    // Check the path provided (is it a file or filder?)

    free_buf(&idx_contents);
    return 0;
}
