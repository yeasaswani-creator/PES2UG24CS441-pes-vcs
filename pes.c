
#include "index.h"
#include "commit.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

// DUMMY FUNCTIONS (to avoid linker errors)
#include <dirent.h>
#include <unistd.h>

// LIST BRANCHES
int branch_list() {
    DIR *dir = opendir(".pes/refs/heads");
    if (!dir) return -1;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue; // skip . and ..
        printf("%s\n", entry->d_name);
    }

    closedir(dir);
    return 0;
}

// CREATE BRANCH
int branch_create(const char *name) {
    char path[512];
    snprintf(path, sizeof(path), ".pes/refs/heads/%s", name);

    // check if already exists
    if (access(path, F_OK) == 0) {
        return -1;
    }

    FILE *src = fopen(".pes/HEAD", "r");
    if (!src) return -1;

    char line[512];
    fgets(line, sizeof(line), src);
    fclose(src);

    char ref_path[512];
    if (strncmp(line, "ref: ", 5) == 0) {
        snprintf(ref_path, sizeof(ref_path), ".pes/%s", line + 5);
        ref_path[strcspn(ref_path, "\n")] = '\0';
    } else {
        return -1;
    }

    FILE *fref = fopen(ref_path, "r");
    char hash[128] = {0};

    if (fref) {
        fgets(hash, sizeof(hash), fref);
        fclose(fref);
    }

    FILE *newb = fopen(path, "w");
    if (!newb) return -1;

    if (strlen(hash) > 0)
        fprintf(newb, "%s", hash);

    fclose(newb);
    return 0;
}

// DELETE BRANCH
int branch_delete(const char *name) {
    char path[512];
    snprintf(path, sizeof(path), ".pes/refs/heads/%s", name);

    return remove(path);
}

// CHECKOUT
int checkout(const char *target) {
    char path[512];
    snprintf(path, sizeof(path), ".pes/refs/heads/%s", target);

    if (access(path, F_OK) != 0) {
        return -1;
    }

    FILE *f = fopen(".pes/HEAD", "w");
    if (!f) return -1;

    fprintf(f, "ref: refs/heads/%s\n", target);
    fclose(f);

    return 0;
}
// ─── LOG CALLBACK FUNCTION ─────────────────────────────────

void print_commit(const ObjectID *id, const Commit *c, void *ctx) {
    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(id, hex);

    printf("commit %s\n", hex);
    printf("Author: %s\n", c->author);
    printf("Message: %s\n\n", c->message);
}

// ─── PROVIDED: Phase 5 Command Wrappers ─────────────────────

void cmd_branch(int argc, char *argv[]) {
    if (argc == 2) {
        branch_list();
    } else if (argc == 3) {
        if (branch_create(argv[2]) == 0) {
            printf("Created branch '%s'\n", argv[2]);
        } else {
            fprintf(stderr, "error: failed to create branch '%s'\n", argv[2]);
        }
    } else if (argc == 4 && strcmp(argv[2], "-d") == 0) {
        if (branch_delete(argv[3]) == 0) {
            printf("Deleted branch '%s'\n", argv[3]);
        } else {
            fprintf(stderr, "error: failed to delete branch '%s'\n", argv[3]);
        }
    } else {
        fprintf(stderr, "Usage:\n  pes branch\n  pes branch <name>\n  pes branch -d <name>\n");
    }
}

void cmd_checkout(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: pes checkout <branch_or_commit>\n");
        return;
    }

    const char *target = argv[2];
    if (checkout(target) == 0) {
        printf("Switched to '%s'\n", target);
    } else {
        fprintf(stderr, "error: checkout failed. Do you have uncommitted changes?\n");
    }
}

// ─── MAIN ───────────────────────────────────────────────────

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: pes <command> [args]\n");
        fprintf(stderr, "\nCommands:\n");
        fprintf(stderr, "  init\n");
        fprintf(stderr, "  add <file>...\n");
        fprintf(stderr, "  status\n");
        fprintf(stderr, "  commit -m <msg>\n");
        fprintf(stderr, "  log\n");
        fprintf(stderr, "  branch\n");
        fprintf(stderr, "  checkout\n");
        return 1;
    }

    const char *cmd = argv[1];

    // ─── INIT ─────────────────────────────
    if (strcmp(cmd, "init") == 0) {
        mkdir(".pes", 0755);
        mkdir(".pes/objects", 0755);
        mkdir(".pes/refs", 0755);
        mkdir(".pes/refs/heads", 0755);

        FILE *f = fopen(".pes/HEAD", "w");
        if (!f) {
            fprintf(stderr, "Failed to create HEAD\n");
            return 1;
        }
        fprintf(f, "ref: refs/heads/main\n");
        fclose(f);

        FILE *b = fopen(".pes/refs/heads/main", "w");
        if (!b) {
            fprintf(stderr, "Failed to create branch\n");
            return 1;
        }
        fclose(b);

        printf("Initialized empty PES repository\n");
    }

    // ─── ADD ──────────────────────────────
    else if (strcmp(cmd, "add") == 0) {
        Index index;
        index_load(&index);

        for (int i = 2; i < argc; i++) {
            if (index_add(&index, argv[i]) == 0) {
                printf("added %s\n", argv[i]);
            }
        }
    }

    // ─── STATUS ───────────────────────────
    else if (strcmp(cmd, "status") == 0) {
        Index index;
        index_load(&index);
        index_status(&index);
    }

    // ─── COMMIT ───────────────────────────
    else if (strcmp(cmd, "commit") == 0) {
        if (argc < 4 || strcmp(argv[2], "-m") != 0) {
            fprintf(stderr, "Usage: pes commit -m <message>\n");
            return 1;
        }

        ObjectID commit_id;
        if (commit_create(argv[3], &commit_id) == 0) {
            char hex[HASH_HEX_SIZE + 1];
            hash_to_hex(&commit_id, hex);
            printf("Committed as %s\n", hex);
        } else {
            fprintf(stderr, "commit failed\n");
        }
    }

    // ─── LOG (FIXED) ──────────────────────
    else if (strcmp(cmd, "log") == 0) {
        ObjectID head;

        if (head_read(&head) != 0) {
            printf("No commits yet\n");
            return 0;
        }

        commit_walk(print_commit, &head);
    }

    // ─── BRANCH ───────────────────────────
    else if (strcmp(cmd, "branch") == 0) {
        cmd_branch(argc, argv);
    }

    // ─── CHECKOUT ─────────────────────────
    else if (strcmp(cmd, "checkout") == 0) {
        cmd_checkout(argc, argv);
    }

    else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        return 1;
    }

    return 0;
}
