#define NOB_IMPLEMENTATION
#include "nob.h"

#define BUILD_FOLDER "build/"
#define SRC_FOLDER   "src/"

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    if (!nob_mkdir_if_not_exists(BUILD_FOLDER)) return 1;

    Nob_Cmd cmd = {0};

    nob_cmd_append(&cmd,
        "gcc",
        "-Wall", "-Wextra", "-pedantic", "-std=c11", "-ggdb",
        "-Wno-deprecated-declarations",
        "-o", BUILD_FOLDER"notsogit",
        SRC_FOLDER"main.c",
        SRC_FOLDER"init.c",
        SRC_FOLDER"hash-object.c",
        SRC_FOLDER"objects.c",
        SRC_FOLDER"cat-file.c",
        SRC_FOLDER"ls-tree.c",
        SRC_FOLDER"write-tree.c",
        "-lcrypto", "-lz"
    );

    if (!nob_cmd_run_sync_and_reset(&cmd)) return 1;
    return 0;
}
