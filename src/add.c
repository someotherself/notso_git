#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "objects.h"
#include "init.h"
#include "index.h"

void sort_index(index_state_t *index) {
    if (index->entries_count == 0) {
        return;
    }
    qsort(&index->entries[0], index->entries_count, sizeof(index_entry_t), cmp_path_key_to_entry);
}

int add(char *path) {
    if (access(path, F_OK) != 0) {
        fprintf(stderr, "Target %s does not exist.\n", path);
        errno = ENOENT;
        return -1;
    } 

    char base[BUFSIZ] = { 0 };
    if (find_base(base, sizeof(base)) == -1) {
        fprintf(stderr, "Could not find repo (%s)\n", strerror(errno));
        return -1;
    }
    // Check if an index already exists and read it in memory
    char index_path[BUFSIZ] = { 0 };
    if (concat_path(index_path, sizeof(index_path), base, "index") < 0) {
        fprintf(stderr, "Path too long: repo/index\n");
        return -1;
    }

    int idx_exists = 0;
    struct stat stat_buf;
    buf_t idx_contents;
    buf_init(&idx_contents);
    if (stat(index_path, &stat_buf) == 0) {
        int index_fd = open(index_path, O_RDONLY);
        if (index_fd >= 0) {
            idx_exists = 1;
            unsigned char tmp[stat_buf.st_size];
            buf_reserve(&idx_contents, stat_buf.st_size);
            ssize_t b_read = read_all(index_fd, tmp, stat_buf.st_size);
            if (b_read < 0 || b_read != stat_buf.st_size) {
                free_buf(&idx_contents);
                close(index_fd);
                return -1;
            }
            buf_append(&idx_contents, tmp, stat_buf.st_size);
            close(index_fd);
        }
    }

    index_state_t index;

    if (idx_exists) {
        // combine, or remove the "index_head_t head;"
        int entries_count;
        if ((entries_count = parse_index_header(&idx_contents)) < -1 ) {
            return -1;
        }
        read_entries(&idx_contents, &index, entries_count);
    } else {
        init_index_state(&index);
    }

    if (read_index_target(path, &index) < 0) {
        errno = EIO;
        return -1;
    }

    if (write_index_to_file(base, &index) == 0) {
        free_index_state(&index);
        free_buf(&idx_contents);
        return -1;
    };

    free_index_state(&index);
    free_buf(&idx_contents);
    return 0;
}
