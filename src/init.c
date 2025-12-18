#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "objects.h"

static int isdir(const char *p) {
    struct stat statbuf = {0};
    if (stat(p, &statbuf) < 0) {
        return -1;
    }
    return S_ISDIR(statbuf.st_mode);
}

int find_base(char *out, size_t out_size) {
    char cur[4096];
    int isdir_ret;

    if (getcwd(cur, sizeof(cur)) == NULL) {
        fprintf(stderr, "Could not find current working directory (%s)\n", strerror(errno));
        return -1;
    }

    int cur_off = strlen(cur) - 1;

    for (;;) {
        int n = snprintf(out, out_size, "%s%s", cur, 
                    (cur[cur_off] == '/') ? ".notso_git" : "/.notso_git");

        if (n < 0 || (size_t)n >= out_size) {
            errno = ENAMETOOLONG;
            return -1;
        }

        isdir_ret = isdir(out);
        if (isdir_ret == 1) {
            return 0;
        }

        /* We have reached root. Exit program */
        if (cur[cur_off] == '/' && cur_off == 0) {
            return -1;
        }
        /* Remove until the next '/' */
        while (cur_off > 0 && cur[cur_off] != '/') {
            cur[cur_off--] = '\0';
        }
        /* Remove duplicate '/' */
        while (cur_off > 0 && cur[cur_off] == '/' && cur[cur_off - 1] == '/') {
            cur[cur_off--] = '\0';
        }
        /* Remove any trailing '/' to cleanup path for next run */
        if (cur_off > 0 && cur[cur_off] == '/' && cur[cur_off - 1] != '/') {
            cur[cur_off--] = '\0';
        }
    }
}

int setup_base_dir(char *root) {
    char base[4096], temp[BUFSIZ];
    int head_fd;

    // .notso_git
    concat_path(base, root, ".notso_git");
    struct stat statbuf = {0};
    if (stat(base, &statbuf) < 0) {
        if (mkdir(base, 0755) < 0) {
            fprintf(stderr, "Could not create .notso_git folder (%s)\n", strerror(errno));
            return -1;
        };
    }
    // HEAD
    snprintf(temp, sizeof(temp), "%s%s", base, "/HEAD");
    if (stat(temp, &statbuf) < 0) {
        head_fd = open(temp, O_WRONLY | O_APPEND | O_CREAT, 0644) ;
        if (head_fd < 0) {
            fprintf(stderr, "Could not create HEAD (%s)\n", strerror(errno));
            return -1;
        };
        close(head_fd);
    }
    // objects/
    snprintf(temp, sizeof(temp), "%s%s", base, "/objects");
    if (stat(temp, &statbuf) < 0) {
        if (mkdir(temp, 0755) < 0) {
            fprintf(stderr, "Could not objects folder (%s)\n", strerror(errno));
            return -1;
        };
    }
    // refs/
    snprintf(temp, sizeof(temp), "%s%s", base, "/refs");
    if (stat(temp, &statbuf) < 0) {
        if (mkdir(temp, 0755) < 0) {
            fprintf(stderr, "Could not create refs folder (%s)\n", strerror(errno));
            return -1;
        };
    }
    return 0;
}

int init(void) {
    char base[4096] = { 0 };

    if (find_base(base, sizeof(base)) == -1) {
        // If base dir does not exist, set it up
        if (getcwd(base, sizeof(base)) == NULL) {
            return 1;
        }
        if (setup_base_dir(base) == -1) {
            return -1;
        };
        printf("Repo initialized\n");
        return 0;
    };

    // Do not re-initialize the repo
    printf("Repo already exists in: %s\n", base);
    return 0;
}
