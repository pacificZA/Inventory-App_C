#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#ifdef _WIN32
    #define PATH_SEP '\\'
#else 
    #define PATH_SEP '/'
    
#endif

// fusionne deux chemins
char* path_join(const char* base, const char* sub) {
    if (!base || !sub) return NULL;

    size_t len_base = strlen(base);
    size_t len_sub = strlen(sub);

    // allocation memoire pour base + separateur + sub + null terminator
    char* result = malloc(len_base + len_sub + 2);
    if (!result) return NULL;

    strcpy(result, base);

    // Ajout separateur si nécéssaire
    if (len_base > 0 && result[len_base - 1] != PATH_SEP) {
        result[len_base] = PATH_SEP;
        result[len_base + 1] = '\0';
    }

    strcat(result, sub);
    return result;
}

//extrait le nom du fichier depuis le chemin 
const char* path_filename(const char* path) {
    if (!path) return NULL;
    const char* last_sep = strrchr(path, PATH_SEP);

#ifdef _WIN32
    // verifie aussi pour in separateur Unix-style dans Windows
    const char* last_sep_alt = strrchr(path, '/');
    if (!last_sep || (last_sep_alt && last_sep_alt > last_sep))
        last_sep = last_sep_alt;

#endif
    return last_sep ? last_sep + 1 : path;
}

// Normalise le separateur de chemin pour la plateforme
void path_normalize(char* path) {
    if (!path) return;

#ifdef _WIN32
    for (char* p = path; *p; p++) {
        if (*p == '\\') *p = '/';
    }
#endif

}

//return une "liste" des fichier/sous-dossier dans le dossier donné en argument
int path_listdir(const char *path, int *count) {
    if (!path) return;

    DIR *dir = opendir(path);

    if (!dir) {
        perror("opendir");
        return 1;
    }

    char **files = NULL;
    int size = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        
        char **tmp = realloc(files, (size + 1) * sizeof(char *));
        if (!tmp) {
            perror("realloc");
            break;
        }

        files = tmp;

        files[size] = malloc(strlen(entry->d_name) + 1);
        if (!files[size]) {
            perror("malloc");
            break;
        }

        strcpy(files[size], entry->d_name);

        size++;
    }

    closedir(dir);

    *count = size;
    return files;
}

/*Utiliser path_listdir: 
    int count;

    char **files = path_listdir([dossier], &count);

    if (files) {
        for (int i = 0; i < count; i++) {
            printf("%s\n", files[i]);
        }
    }

    // Libération de la mémoire
    for (int i = 0; i < count; i++) {
        free(files[i]);
    }

    free(files);
*/