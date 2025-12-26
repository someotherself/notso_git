#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include "init.h"
#include "add.h"
#include "hash-object.h"
#include "cat-file.h"
#include "write-tree.h"
#include "ls-tree.h"

#ifndef __linux__
int main(void) {
    fprintf(stderr, "This program only runs on Linux\n");
    return 1;
}
#endif

typedef enum {
    // Info on generic usage
    USAGE_GEN = 0,
    USAGE_INIT,
    USAGE_ADD,
    USAGE_HASH_OBJECT,
    USAGE_CAT_FILE,
    USAGE_LS_TREE,
    USAGE_WRITE_TREE,
    USAGE_LS_FILES,
    // Command not recognized
    USAGE_INVALID,
} usage_t;

void usage(usage_t u) {
    switch (u) {
        case USAGE_GEN:
            fprintf(stderr,
                "usage: ./notsogit <command> [<args>]\n");
            fprintf(stderr,
                "Commands: init, add, hash-object, cat-file, ls-tree\n");
            break;
        case USAGE_INIT:
            fprintf(stderr,
                "usage: ./notsogit init\n"
                "Create an empty repository.\n");
            break;
        case USAGE_ADD:
            fprintf(stderr,
                "usage: ./notsogit add <path>\n"
                "Add file contents to the staging area.\n");
            break;
        case USAGE_HASH_OBJECT:
            fprintf(stderr,
                "usage: ./notsogit hash-object [-w] <file>\n"
                "Compute object ID and optionally write object to database.\n");
            break;
        case USAGE_CAT_FILE:
            fprintf(stderr,
                "usage: ./notsogit cat-file -p <object>\n"
                "Pretty-print object contents.\n");
            break;
        case USAGE_LS_TREE:
            fprintf(stderr,
                "usage: ./notsogit ls-tree <tree>\n"
                "List the contents of a tree object (writing trees not implemented).\n");
            break;
        case USAGE_WRITE_TREE:
            fprintf(stderr,
                "usage: ./notsogit write-tree\n"
                "Create a tree object from the staging area.\n");
            break;
        case USAGE_LS_FILES:
            fprintf(stderr,
                "usage: ./notsogit ls-files\n"
                "Show information about the files in the staging area.\n");
            break;
        case USAGE_INVALID:
            fprintf(stderr, "Not a valid command. Use --help for a list of commands.\n");
            break;
        default:
            break;
        }
}

int run(int argc, char **argv) {
    if (argc < 2) {
        usage(USAGE_GEN);
        return 1;
    }

    if (strcmp(argv[1], "--help") == 0) {
        usage(USAGE_GEN);
        return 1;
    }

    if (strcmp(argv[1], "init") == 0) {
        if (argc > 2) {
            usage(USAGE_INIT);
            return 1;
        }
        init();
        return 0;
    }

    if (strcmp(argv[1], "add") == 0) {
        if (argc != 3) {
            usage(USAGE_ADD);
            return 1;
        }
        if (strcmp(argv[2], "--help") == 0) {
            usage(USAGE_ADD);
            return 1;
        }
        char path[MAX_PATH];
        if (strlen(argv[2]) >= sizeof(path)) {
            fprintf(stderr, "Input is too long\n");
            return -1;
        }
        strncpy(path, argv[2], sizeof(path) - 1);
        add(path);
        return 0;
    }

    if (strcmp(argv[1], "hash-object") == 0) {
        int write = 0;
        char file_path[MAX_PATH];

        if (argc < 3 || argc > 4) {
            usage(USAGE_HASH_OBJECT);
            return 1;
        }

        oid_t oid = { 0 };

        if (argc == 3) {
            if (strcmp(argv[2], "--help") == 0) {
                usage(USAGE_HASH_OBJECT);
                return 1;
            }
            if (strlen(argv[2]) >= sizeof(file_path)) {
                fprintf(stderr, "Input is too long\n");
                return -1;
            }
            strncpy(file_path, argv[2], sizeof(file_path) - 1);
            hash_object(&oid, write, file_path);
            print_hex(&oid);
            return 0;
        }

        if (strcmp(argv[2], "-w") == 0) {
            if (strcmp(argv[3], "--help") == 0) {
                usage(USAGE_HASH_OBJECT);
                return 1;
            }
            write = 1; /* Write the hashed object to file */
            if (strlen(argv[3]) >= sizeof(file_path)) {
                fprintf(stderr, "Input is too long\n");
                return -1;
            }
            strncpy(file_path, argv[3], sizeof(file_path) - 1);
            hash_object(&oid, write, file_path);
            print_hex(&oid);
            return 0;
        }

        /* if still have 4 args and 3rd one isn't -w, return */
        if (argc == 4) {
            usage(USAGE_HASH_OBJECT);
            return 1;
        } 
    }

    if (strcmp(argv[1], "cat-file") == 0) {
        int pretty = 0;
        char oid_name[4096];
        if (argc < 3 || argc > 4 || strcmp(argv[2], "--help") == 0) {
            usage(USAGE_CAT_FILE);
            return 1;
        }

        if (argc == 3) {
            if (strlen(argv[2]) > 40) {
                // However, searching short hashes is not implemented
                fprintf(stderr, "Invalid input\n");
                return -1;
            }
            strncpy(oid_name, argv[2], sizeof(oid_name) - 1);
            cat_file(oid_name, pretty);
            return 0;
        }

        if (strcmp(argv[2], "-p") == 0) {
            if (strcmp(argv[3], "--help") == 0) {
                usage(USAGE_CAT_FILE);
                return 1;
            }
            pretty = 1;
            if (strlen(argv[3]) > 40) {
                fprintf(stderr, "Invalid input\n");
                return -1;
            }
            strncpy(oid_name, argv[3], sizeof(oid_name) - 1);
            cat_file(oid_name, pretty);
            return 0;
        }

        /* if still have 4 args and 3rd one isn't -p, return */
        if (argc == 4) {
            usage(USAGE_CAT_FILE);
            return 1;
        } 
    }

    if (strcmp(argv[1], "write-tree") == 0) {
        if (argc > 2) {
            usage(USAGE_WRITE_TREE);
            return 1;
        }
        printf("Not implemented\n");
        // write_tree();
        return 0;
    }

    if (strcmp(argv[1], "ls-tree") == 0) {
        char oid_name[4096];
        if (argc != 3 || strcmp(argv[2], "--help") == 0) {
            usage(USAGE_LS_TREE);
            return 1;
        }
        if (strlen(argv[2]) > 40) {
            fprintf(stderr, "Invalid input\n");
            return -1;
        }
        strncpy(oid_name, argv[2], sizeof(oid_name) - 1);
        ls_tree(oid_name);
        return 0;
    }

    if (strcmp(argv[1], "ls-files") == 0) {
        if (argc == 3 && strcmp(argv[2], "--help") == 0) {
            usage(USAGE_LS_FILES);
            return 1;
        }
        if (argc > 2) {
            usage(USAGE_LS_FILES);
            return 1;
        }
        ls_files();
        return 0;
    }

    usage(USAGE_INVALID);
    return 0;
}

int main(int argc, char **argv) {
    if (run(argc, argv) == -1) {
        fprintf(stderr, "Error: (%s)\n", strerror(errno));
        return -1;
    }

    return 0;
}
