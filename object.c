#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <openssl/sha.h>

// ─── PROVIDED ────────────────────────────────────────────────────────────────

void hash_to_hex(const ObjectID *id, char *hex_out) {
    for (int i = 0; i < HASH_SIZE; i++) {
        sprintf(hex_out + i * 2, "%02x", id->hash[i]);
    }
    hex_out[HASH_HEX_SIZE] = '\0';
}

int hex_to_hash(const char *hex, ObjectID *id_out) {
    if (strlen(hex) < HASH_HEX_SIZE) return -1;
    for (int i = 0; i < HASH_SIZE; i++) {
        unsigned int byte;
        if (sscanf(hex + i * 2, "%2x", &byte) != 1) return -1;
        id_out->hash[i] = (uint8_t)byte;
    }
    return 0;
}

void compute_hash(const void *data, size_t len, ObjectID *id_out) {
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, data, len);
    SHA256_Final(id_out->hash, &ctx);
}

void object_path(const ObjectID *id, char *path_out, size_t path_size) {
    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(id, hex);
    snprintf(path_out, path_size, "%s/%.2s/%s", OBJECTS_DIR, hex, hex + 2);
}

int object_exists(const ObjectID *id) {
    char path[512];
    object_path(id, path, sizeof(path));
    return access(path, F_OK) == 0;
}

// ─── YOUR IMPLEMENTATION ─────────────────────────────────────────────────────

// convert enum to string
const char* type_to_str(ObjectType type) {
    if (type == OBJ_BLOB) return "blob";
    if (type == OBJ_TREE) return "tree";
    if (type == OBJ_COMMIT) return "commit";
    return "";
}

// convert string to enum
ObjectType str_to_type(const char *str) {
    if (strcmp(str, "blob") == 0) return OBJ_BLOB;
    if (strcmp(str, "tree") == 0) return OBJ_TREE;
    if (strcmp(str, "commit") == 0) return OBJ_COMMIT;
    return -1;
}

// WRITE OBJECT
int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out) {

    const char *type_str = type_to_str(type);

    char header[64];
    int header_len = sprintf(header, "%s %zu", type_str, len) + 1;

    size_t total_size = header_len + len;
    unsigned char *buffer = malloc(total_size);

    memcpy(buffer, header, header_len);
    memcpy(buffer + header_len, data, len);

    // compute hash
    compute_hash(buffer, total_size, id_out);

    // check if exists
    if (object_exists(id_out)) {
        free(buffer);
        return 0;
    }

    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(id_out, hex);

    // create directories
    mkdir(".pes", 0755);
    mkdir(".pes/objects", 0755);

    char dir[3];
    strncpy(dir, hex, 2);
    dir[2] = '\0';

    char dir_path[512];
    snprintf(dir_path, sizeof(dir_path), "%s/%s", OBJECTS_DIR, dir);
    mkdir(dir_path, 0755);

    // final path
    char final_path[512];
    snprintf(final_path, sizeof(final_path), "%s/%s", dir_path, hex + 2);

    // temp file
    char tmp_path[512];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", final_path);

    int fd = open(tmp_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        free(buffer);
        return -1;
    }

    write(fd, buffer, total_size);
    fsync(fd);
    close(fd);

    rename(tmp_path, final_path);

    free(buffer);
    return 0;
}

// READ OBJECT
int object_read(const ObjectID *id, ObjectType *type_out, void **data_out, size_t *len_out) {

    char path[512];
    object_path(id, path, sizeof(path));

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    rewind(f);

    unsigned char *buffer = malloc(size);
    fread(buffer, 1, size, f);
    fclose(f);

    // verify hash
    ObjectID new_id;
    compute_hash(buffer, size, &new_id);

    if (memcmp(new_id.hash, id->hash, HASH_SIZE) != 0) {
        free(buffer);
        return -1;
    }

    // parse header
    char *null_pos = memchr(buffer, '\0', size);
    if (!null_pos) {
        free(buffer);
        return -1;
    }

    char header[64];
    size_t header_len = null_pos - (char*)buffer;
    memcpy(header, buffer, header_len);
    header[header_len] = '\0';

    char type_str[16];
    size_t data_size;
    sscanf(header, "%s %zu", type_str, &data_size);

    *type_out = str_to_type(type_str);

    char *data_start = null_pos + 1;

    *len_out = data_size;
    *data_out = malloc(data_size);
    memcpy(*data_out, data_start, data_size);

    free(buffer);
    return 0;
}
