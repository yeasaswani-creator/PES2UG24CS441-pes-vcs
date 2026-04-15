#include "pes.h"
#include "index.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

// 🔥 DUMMY FUNCTIONS (to avoid linker errors)

int branch_list() { return 0; }
int branch_create(const char *name) { return 0; }
int branch_delete(const char *name) { return 0; }
int checkout(const char *target) { return 0; }

// ─── PROVIDED: Phase 5 Command Wrappers ─────────────────────────────────────

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

// ─── PROVIDED: Command dispatch ─────────────────────────────────────────────

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: pes <command> [args]\n");
        fprintf(stderr, "\nCommands:\n");
        fprintf(stderr, "  init            Create a new PES repository\n");
        fprintf(stderr, "  add <file>...   Stage files for commit\n");
        fprintf(stderr, "  status          Show working directory status\n");
        fprintf(stderr, "  commit -m <msg> Create a commit from staged files\n");
        fprintf(stderr, "  log             Show commit history\n");
        fprintf(stderr, "  branch          List, create, or delete branches\n");
        fprintf(stderr, "  checkout <ref>  Switch branches or restore working tree\n");
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "init") == 0) {
        mkdir(".pes", 0755);
        mkdir(".pes/objects", 0755);
        mkdir(".pes/refs", 0755);
        mkdir(".pes/refs/heads", 0755);

        printf("Initialized empty PES repository\n");
    }
    else if (strcmp(cmd, "add") == 0) {
        Index index;
        index_load(&index);

        for (int i = 2; i < argc; i++) {
            if (index_add(&index, argv[i]) == 0) {
                printf("added %s\n", argv[i]);
            }
        }
    }
    else if (strcmp(cmd, "status") == 0) {
        Index index;
        index_load(&index);
        index_status(&index);
    }
    else if (strcmp(cmd, "commit") == 0) {
        printf("commit not implemented\n");
    }
    else if (strcmp(cmd, "log") == 0) {
        printf("log not implemented\n");
    }
    else if (strcmp(cmd, "branch") == 0) {
        cmd_branch(argc, argv);
    }
    else if (strcmp(cmd, "checkout") == 0) {
        cmd_checkout(argc, argv);
    }
    else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        fprintf(stderr, "Run 'pes' with no arguments for usage.\n");
        return 1;
    }

    return 0;
}
