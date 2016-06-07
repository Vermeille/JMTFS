#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define K4 (4 * 1024)
#define DISK_SIZE (512 * 1024 * 1024)
#define NB_TOTAL_CLUSTERS (DISK_SIZE / K4)
#define FAT_NB_CLUSTERS (NB_TOTAL_CLUSTERS / 1025)
#define FAT_SIZE (FAT_NB_CLUSTERS * K4)
#define NB_DATA_CLUSTERS (FAT_NB_CLUSTERS * 1024)

typedef unsigned char u8;

u8 g_disk[DISK_SIZE];

unsigned int* get_fat() {
    return (unsigned int*)g_disk;
}

u8* get_cluster(int nb) {
    return &g_disk[FAT_SIZE + nb * K4];
}

int is_cluster_free(int nb) {
    return get_fat()[nb] == -1;
}

int is_cluster_eof(int nb) {
    return get_fat()[nb] == 0;
}

int get_next_cluster(int nb) {
    return get_fat()[nb];
}

void set_cluster_used(int nb) {
    get_fat()[nb] = 0;
}

void set_next_cluster(int nb, int next) {
    get_fat()[nb] = next;
}

void set_cluster_free(int nb) {
    get_fat()[nb] = -1;
}

int find_first_free_cluster() {
    for (int i = 0; i < NB_DATA_CLUSTERS; ++i) {
        if (is_cluster_free(i)) {
            return i;
        }
    }
    return -1;
}

int allocate_cluster() {
    int clu = find_first_free_cluster();
    if (clu == -1) {
        return -1;
    }
    set_cluster_used(clu);
    return clu;
}

void free_cluster_chain(int start) {
    while (!is_cluster_eof(start)) {
        int next = get_next_cluster(start);
        set_cluster_free(start);
        start = next;
    }
}

enum Type {
    TYPE_DIR,
    TYPE_FILE,
};

struct File {
    enum Type type;
    char name[64];
    unsigned int sz;
    union {
        unsigned int file_start_cluster;
        unsigned int dir_next_cluster;
    };
    unsigned int files_in_dir[128];
};

int get_dirname_end(const char* path) {
    int i = 0;
    while (path[i] == '/') {
        ++i;
    }
    while (path[i] && path[i] != '/') {
        ++i;
    }
    return i;
}

int get_path_next_elem(const char* path) {
    int i = get_dirname_end(path);
    if (path[i] != '/') {
        return -1;
    }

    while (path[i] == '/') {
        ++i;
    }
    return i - 1;
}

int is_composed_path(const char* p) {
    while (*p == '/') {
        ++p;
    }

    while (*p && *p != '/') {
        ++p;
    }
    return *p == '/';
}

const char* basename(const char* path) {
    const char* last_start = path;
    while (*path) {
        while (*path == '/') {
            last_start = path;
            ++path;
        }
        while (*path && *path != '/') {
            ++path;
        }
    }
    return last_start;
}

struct File* dir_get_nth_file(struct File* dir, int n) {
    return (struct File*)get_cluster(dir->files_in_dir[n]);
}

struct File* get_file_in_dir(struct File* dir, const char* name) {
    // FIXME: handle when files span across several clusters
    for (int i = 0; i < dir->sz; ++i) {
        if(strcmp(dir_get_nth_file(dir, i)->name, name) == 0) {
            return dir_get_nth_file(dir, i);
        }
    }
    return NULL;
}

int allocate_file(const char* name, int here, enum Type type) {
    struct File* f = (struct File*)get_cluster(here);
    f->type = type;
    strncpy(f->name, name, 63);
    f->name[63] = '\0';
    f->sz = 0;
    if (type == TYPE_DIR) {
        f->dir_next_cluster = 0;
    } else {
        f->file_start_cluster = -1;
    }
    return here;
}

struct File* find_parent_dir(const char* ro_path) {
    struct File* parent_dir = (struct File*)get_cluster(0);

    if (strcmp(ro_path, "/") == 0) {
        return parent_dir;
    }

    char* saved_path = strdup(ro_path);
    char* name = saved_path;
    while (is_composed_path(name)) {
        int slash_idx = get_dirname_end(name);
        name[slash_idx] = '\0';
        parent_dir = get_file_in_dir(parent_dir, name + 1);
        if (!parent_dir || parent_dir->type == TYPE_FILE) {
            return NULL;
        }
        name[slash_idx] = '/';
        name = get_path_next_elem(name) + name;
    }
    free(saved_path);
    return parent_dir;
}

struct File* find_file(const char* ro_path) {
    struct File* f = find_parent_dir(ro_path);
    if (strcmp(ro_path, "/") == 0) {
        return f;
    }

    if (!f || f->type == TYPE_FILE) {
        return NULL;
    }
    f = get_file_in_dir(f, basename(ro_path) + 1);
    if (!f || f->type == TYPE_FILE) {
        return NULL;
    }
    return f;
}

void add_file_to_dir(struct File* f, int new_file_cluster) {
    f->files_in_dir[f->sz] = new_file_cluster;
    ++f->sz;
}

int create_file(const char* name, enum Type type) {
    struct File* parent = find_parent_dir(name);
    if (!parent) {
        fprintf(stderr, "can't find parent directory\n");
        return -1;
    }

    if (get_file_in_dir(parent, basename(name) + 1) != NULL) {
        fprintf(stderr, "can't create %s, already existing", name);
        return -1;
    }

    int clu = allocate_cluster();
    if (clu == -1) {
        return -1;
    }

    allocate_file(basename(name) + 1, clu, type);
    add_file_to_dir(parent, clu);
}

void mkdir(const char* name) {
    create_file(name, TYPE_DIR);
}

void touch(const char* name) {
    create_file(name, TYPE_FILE);
}

void init() {
    for (int i = 0; i < NB_DATA_CLUSTERS; ++i) {
        set_cluster_free(i);
    }
    if (allocate_cluster() != 0) {
        fprintf(stderr, "Cannot allocate cluster 0 for root dir\n");
        return;
    }
    allocate_dir("", 0);
}

void ls(const char* path) {
    struct File* dir = find_file(path);
    if (dir == NULL) {
        fprintf(stderr, "invalid path\n");
        return;
    }

    if (dir-> type == TYPE_DIR) {
        printf("%s:\n", path);
        for (int i = 0; i < dir->sz; ++i) {
            printf("%s\n", dir_get_nth_file(dir, i)->name);
        }
    } else {
        printf("%s:\n", path);
    }
}

void write(const char* file, const char* data, unsigned int size) {
    struct File* f = find_file(file);
    if (!f) {
        fprintf(stderr, "cannot open file %s\n", file);
        return;
    }
    f->sz = size;
    if (f->file_start_cluster == -1) {
        f->file_start_cluster = allocate_cluster();
    }
    int current_cluster = f->file_start_cluster;
    while (size >= 0) {
        memcpy(get_cluster(current_cluster), data, min(size, K4));
        size -= min(K4, size);
        data += min(K4, size);
        if (size > 0) {
            int next_cluster = allocate_cluster();
            set_next_cluster(current_cluster, next_cluster);
            current_cluster = next_cluster;
        }
    }
}

int main() {
    init();
    ls("/");
    printf("---\n");
    mkdir("/foo");
    mkdir("/bar");
    mkdir("/test");
    ls("/");
    printf("---\n");
    mkdir("/foo/lol");
    mkdir("/foo/test");
    ls("/");
    ls("/foo");
    return 0;
}
