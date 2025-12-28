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

// Compare function used by qsort to to sort index entries by path.
static int cmp_entry_path(const void *a, const void *b) {
    const index_entry_t *ea = (const index_entry_t *)a;
    const index_entry_t *eb = (const index_entry_t *)b;
    return strcmp(ea->path, eb->path);
}

/// @brief Helper for the qsort function in stdlib. Sorts the index entries by path.
/// @param index In memory index state
void sort_index(index_state_t *index) {
    if (index->entries_count == 0) {
        return;
    }
    qsort(&index->entries[0], index->entries_count, sizeof(index_entry_t), cmp_entry_path);
}

/// @brief Prepare the in memory index state. If index exists on disk, it reads it into memory. Otherwise, index is initialized.
/// @param base Path to the notso_git repository
/// @param index Empty in memory index.
/// @return -1 on error.
int init_index(char *base, index_state_t *index) {
    // Check if an index already exists and read it in memory
    char index_path[BUFSIZ] = { 0 };
    if (concat_path(index_path, sizeof(index_path), base, "index") < 0) {
        fprintf(stderr, "Path too long: repo/index\n");
        return -1;
    }

    buf_t idx_contents;
    buf_init(&idx_contents);

    int idx_exists = 0;
    struct stat stat_buf;

    int index_fd;
    if ((index_fd = open(index_path, O_RDONLY)) >= 0 &&
            (fstat(index_fd, &stat_buf) == 0)) {
        idx_exists = 1;
        buf_reserve(&idx_contents, stat_buf.st_size);
        ssize_t b_read = read_all(index_fd, idx_contents.data, stat_buf.st_size);
        if (b_read < 0 || b_read != stat_buf.st_size) {
            free_buf(&idx_contents);
            close(index_fd);
            return -1;
        }
        idx_contents.len += b_read;
        close(index_fd);
    }

    init_index_state(index);

    if (idx_exists) {
        int entries_count;
        if ((entries_count = parse_index_header(&idx_contents)) < -1 ) {
            free_buf(&idx_contents);
            return -1;
        }
        if (read_entries(&idx_contents, index, entries_count) < 0) {
            free_buf(&idx_contents);
            return -1;
        };
    }

    free_buf(&idx_contents);
    return 0;
}

int add(char *path) {
    if (access(path, F_OK) != 0) {
        fprintf(stderr, "Target %s does not exist.\n", path);
        errno = ENOENT;
        return -1;
    } 

    char base[BUFSIZ] = { 0 };
    if (find_base(base, sizeof(base)) < 0) {
        fprintf(stderr, "Could not find repo (%s)\n", strerror(errno));
        return -1;
    }

    index_state_t index = { 0 };

    if (init_index(base, &index) < 0) {
        free_index_state(&index);
        return -1;
    }

    if (read_index_target(path, &index) < 0) {
        free_index_state(&index);
        errno = EIO;
        return -1;
    }

    if (write_index_to_file(base, &index) < 0) {
        free_index_state(&index);
        return -1;
    };

    free_index_state(&index);
    return 0;
}
