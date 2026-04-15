#include "index.h"
#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

IndexEntry* index_find(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0) {
            return &index->entries[i];
        }
    }
    return NULL;
}

int index_remove(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0) {
            int remaining = index->count - i - 1;
            if (remaining > 0) {
                memmove(&index->entries[i], &index->entries[i + 1], remaining * sizeof(IndexEntry));
            }
            index->count--;
            return index_save(index);
        }
    }
    fprintf(stderr, "error: '%s' is not in the index\n", path);
    return -1;
}

int index_status(const Index *index) {
    printf("Staged changes:\n");
    if (index->count == 0) {
        printf("  (nothing to show)\n\n");
    } else {
        for (int i = 0; i < index->count; i++) {
            printf("  staged: %s\n", index->entries[i].path);
        }
        printf("\n");
    }

    printf("Unstaged changes:\n");
    int unstaged = 0;

    for (int i = 0; i < index->count; i++) {
        struct stat st;
        if (stat(index->entries[i].path, &st) != 0) {
            printf("  deleted: %s\n", index->entries[i].path);
            unstaged++;
        } else {
            if ((uint64_t)st.st_mtime != index->entries[i].mtime_sec || (uint32_t)st.st_size != index->entries[i].size) {
                printf("  modified: %s\n", index->entries[i].path);
                unstaged++;
            }
        }
    }

    if (unstaged == 0) printf("  (nothing to show)\n");
    printf("\n");

    printf("Untracked files:\n");

    DIR *dir = opendir(".");
    if (!dir) return -1;

    struct dirent *ent;
    int untracked = 0;

    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;
        if (strcmp(ent->d_name, ".pes") == 0)
            continue;

        if (strstr(ent->d_name, ".o")) continue;
        if (strstr(ent->d_name, ".png")) continue;
        if (strstr(ent->d_name, ".jpeg")) continue;
        if (strcmp(ent->d_name, "pes") == 0) continue;
        if (strcmp(ent->d_name, "Makefile") == 0) continue;
        if (strcmp(ent->d_name, "README.md") == 0) continue;
        if (strcmp(ent->d_name, ".gitignore") == 0) continue;
        if (strstr(ent->d_name, ".swp")) continue;

        int tracked = 0;
        for (int i = 0; i < index->count; i++) {
            if (strcmp(index->entries[i].path, ent->d_name) == 0) {
                tracked = 1;
                break;
            }
        }

        if (!tracked) {
            struct stat st;
            if (stat(ent->d_name, &st) == 0 && S_ISREG(st.st_mode)) {
                printf("  %s\n", ent->d_name);
                untracked++;
            }
        }
    }

    closedir(dir);

    if (untracked == 0) printf("  (nothing to show)\n");

    return 0;
}

int index_load(Index *index) {
    FILE *fp = fopen(".pes/index", "r");
    if (!fp) {
        index->count = 0;
        return 0;
    }

    index->count = 0;

    while (1) {
        IndexEntry e;
        char hex[HASH_HEX_SIZE + 1];

        if (fscanf(fp, "%o %s %lu %u %s\n",
                   &e.mode,
                   hex,
                   &e.mtime_sec,
                   &e.size,
                   e.path) != 5) {
            break;
        }

        hex_to_hash(hex, &e.hash);
        index->entries[index->count++] = e;
    }

    fclose(fp);
    return 0;
}

int index_save(const Index *index) {
    mkdir(".pes", 0755);

    FILE *fp = fopen(".pes/index.tmp", "w");
    if (!fp) return -1;

    for (int i = 0; i < index->count; i++) {
        char hex[HASH_HEX_SIZE + 1];
        hash_to_hex(&index->entries[i].hash, hex);

        fprintf(fp, "%o %s %lu %u %s\n",
                index->entries[i].mode,
                hex,
                index->entries[i].mtime_sec,
                index->entries[i].size,
                index->entries[i].path);
    }

    fclose(fp);
    rename(".pes/index.tmp", ".pes/index");

    return 0;
}

int index_add(Index *index, const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        fprintf(stderr, "error: file not found\n");
        return -1;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;

    void *data = malloc(st.st_size);
    fread(data, 1, st.st_size, fp);
    fclose(fp);

    ObjectID oid;
    object_write(OBJ_BLOB, data, st.st_size, &oid);

    free(data);

    IndexEntry *existing = index_find(index, path);

    if (existing) {
        existing->hash = oid;
        existing->mtime_sec = st.st_mtime;
        existing->size = st.st_size;
        existing->mode = st.st_mode;
    } else {
        IndexEntry e;
        e.hash = oid;
        e.mtime_sec = st.st_mtime;
        e.size = st.st_size;
        e.mode = st.st_mode;
        strncpy(e.path, path, sizeof(e.path));
        index->entries[index->count++] = e;
    }

    return index_save(index);
}
