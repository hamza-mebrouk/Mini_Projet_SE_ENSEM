/*
 * Auteur : Hamza Mebrouk — ENSEM 2026
 */
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <dirent.h>
#include "../include/scheduler.h"

/* Handles des bibliothèques dynamiques chargées */
static void *dl_handles[MAX_POLICIES];

void registry_load(PolicyRegistry *reg, const char *dir) {
    reg->count = 0;
    DIR *d = opendir(dir);
    if (!d) {
        fprintf(stderr, "Avertissement: répertoire de politiques '%s' inaccessible\n", dir);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && reg->count < MAX_POLICIES) {
        /* Chercher les .so */
        const char *name = entry->d_name;
        size_t len = strlen(name);
        if (len <= 3 || strcmp(name + len - 3, ".so") != 0)
            continue;

        char path[512];
        snprintf(path, sizeof(path), "%s/%s", dir, name);

        void *handle = dlopen(path, RTLD_NOW);
        if (!handle) {
            fprintf(stderr, "dlopen('%s'): %s\n", path, dlerror());
            continue;
        }

        SchedulerFunc f = (SchedulerFunc)dlsym(handle, "schedule");
        if (!f) {
            fprintf(stderr, "dlsym('%s', schedule): %s\n", path, dlerror());
            dlclose(handle);
            continue;
        }

        dl_handles[reg->count] = handle;
        reg->entries[reg->count].func = f;

        /* Nom = nom du fichier sans extension */
        strncpy(reg->entries[reg->count].name, name, len - 3);
        reg->entries[reg->count].name[len - 3] = '\0';
        reg->count++;
    }
    closedir(d);
}

Policy *registry_find(PolicyRegistry *reg, const char *name) {
    for (int i = 0; i < reg->count; i++)
        if (strcasecmp(reg->entries[i].name, name) == 0)
            return &reg->entries[i];
    return NULL;
}

int menu_select(const PolicyRegistry *reg) {
    printf("\n╔══════════════════════════════════════╗\n");
    printf("║   POLITIQUES D'ORDONNANCEMENT       ║\n");
    printf("╠══════════════════════════════════════╣\n");
    for (int i = 0; i < reg->count; i++)
        printf("║  [%d] %-32s║\n", i + 1, reg->entries[i].name);
    printf("╠══════════════════════════════════════╣\n");
    printf("║  [0] Quitter                         ║\n");
    printf("╚══════════════════════════════════════╝\n");
    printf("Votre choix : ");

    int choice = 0;
    if (scanf("%d", &choice) != 1) return -1;
    if (choice <= 0 || choice > reg->count) return -1;
    return choice - 1;
}
