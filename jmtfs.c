#include <stdlib.h>
#include <stdio.h>

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

void init() {
    for (int i = 0; i < NB_DATA_CLUSTERS; ++i) {
        set_cluster_free(i);
    }
}

int main() {
    init();
    return 0;
}
