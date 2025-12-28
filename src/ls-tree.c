#include <stdio.h>
#include <string.h>

#include "objects.h"

/// @brief Used by cat-file to parse and print contents on a tree object.
/// @param contents Raw, decompressed contents of the tree object.
/// @param content_len Contents length from the object header.
/// @return -1 on error
int read_tree(unsigned char *contents, size_t content_len) {
    const unsigned char *cursor = (unsigned char*)contents; // cursor needs to track bytes
    const unsigned char *end = cursor + content_len;

    while (cursor < end) {
        // GIT MODE
        unsigned char *space = memchr(cursor, ' ', (size_t)(end - cursor));
        if (space == NULL) {
            return -1;
        };

        size_t mode_len = (size_t)(space - cursor);
        if (mode_len > 16) {
            return -1;
        }
        // TODO: Null terminate it
        char mode[17];
        memcpy(mode, cursor, mode_len);
        mode[mode_len] = '\0';
        char *mode_str = mode_to_str(mode);
        cursor += mode_len + 1;

        unsigned mode_val = 0;
        sscanf(mode, "%o", &mode_val);

        // walk to the first "\0"
        unsigned char *nul = memchr(cursor, '\0', (size_t)(end - cursor + 1));
        if (nul == NULL) {
            return -1;
        };
        // FILE NAME
        char name[MAX_PATH] = { 0 };
        size_t name_len = (size_t)(nul - cursor);
        memcpy(name, cursor, name_len);
        name[name_len] = '\0';
        cursor = nul + 1;

        // HASH
        unsigned char oid[20] = { 0 };
        memcpy(oid, cursor, 20);
        cursor += 20;

        char hex[41];
        oid_to_hex(oid, hex);

        printf("%06o %s %s\t%s\n", mode_val, mode_str, hex, name);
    }

    return 0;
}

int ls_tree(char *oid_name) {
    UNUSED(oid_name);
    return 0;
}