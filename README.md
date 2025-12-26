# A very minimal git clone written in C

Work in progress. Only the core git commands are partially implemented.

# How to run

```bash
git clone https://github.com/someotherself/notso_git.git && cd notso_git
```

Compile the build tool
```bash
cc -o nob nob.c
```

Build the project
```bash
./nob
```

Run it
```bash
./build/notsogit init
```

Commands implemented:
```text
init - Initializes a repository under ./.notso_git
add - Add files to the staging area. No flags available.
hash-object - Create blobs. '-w' flag is available to write to disk.
cat-file - View git objects. Works for blobs and tree. However, creating trees is not yet implemented.
ls-files - List contents from the index file.
```