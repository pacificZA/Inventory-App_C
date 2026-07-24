#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lib/cJSON.h"
#include "lib/path.h"
#ifdef _WIN32
    #include <direct.h>
#else
    #include <sys/stat.h>
#endif

//apres un path_join TOUJOURS FAIRE free(nom de la variable)
//apres une utilisation json aussi faire cJSON_Delete()

char *appdata = NULL;
char *path = NULL;
char *file = NULL;
char *params = NULL;
cJSON *data = NULL;

/* Verifie la validité d'un nom de fichier! ne contient pas ``/\:*"<>|`` ou n'est pas vide 
renvoie 0 si c'est bon sinon 1*/
int check_name(const char *filename) {
    if (!filename || filename[0] == '\0') return 1;

    const char prob[] = "/\\:*\"<>|";
    size_t size = strlen(filename);

    if (strcspn(filename, prob) == size) { // strcspn donne la longeur avant de renconter un character du deuxieme arg 
        return 0;                          // donc si = -> pas de char en commun
    }
    printf("Error: Invalid file name (can't contain: /\\:*\"<>|)\n");
    return 1;
}

// change de fichier d'inventaire et modifie params.json
void switch_file(const char *name) {
    if (check_name(name) == 1) return;

    char *selected;
    size_t len = strlen(name);

    if (len >= 5 && strcmp(name + len - 5, ".json") == 0) {
        selected = strdup(name);    // Si le fichier se termine en .json -> fait rien
    } else {
        selected = malloc(len + 6);
        sprintf(selected, "%s.json", name); // Sinon -> l'ajoute
    }

    if (strcmp(selected, "params.json") == 0) {
        printf("Error: can't switch to the parameter file\n"); // Si le nom est params.json -> refuse
        free(selected);
        return;
    }

    update(file);

    char *full_path = path_join(path, selected);

    FILE *f = fopen(full_path, "r");
    if (!f) {
        printf("Error: Inventory file '%s' not found\n", name);
        free(selected); // Si fichier non ouvert (pas trouvé/bug)
        free(full_path);
        return;
    }

    FILE *fp = fopen(params, "r");
    if (!fp) {
        printf("Error: cannot open parameter file\n");
        free(selected); // Si fichier non ouvert (pas trouvé/ bug)
        free(full_path);
        fclose(f);
        return;
    }

    char buffer[1024];
    len = fread(buffer, 1, sizeof(buffer)-1, f);    //creer un buffer qui stock l'interieur du fichier avant de le parser
    buffer[len] = '\0';

    cJSON_Delete(data);
    data = cJSON_Parse(buffer);
    fclose(f);

    if (!data || !cJSON_IsArray(data)) {
        printf("Error: The inventory file is invalid. Opening an empty inventory.\n");
        cJSON_Delete(data);
        data = cJSON_CreateArray(); //verifie que data != NULL et que c'est bien un array (une liste)
        fclose(fp);
        free(selected);
        free(full_path);
        return;
    }

    buffer[0] = '\0';
    len = fread(buffer, 1, sizeof(buffer)-1, fp); // reset le buffer d'avant pour réutiliser
    buffer[len] = '\0';

    cJSON *param = cJSON_Parse(buffer);
    if (!param) {
        printf("Error: invalid params file\n"); // Si le parse marche pas (pas un json dcp)
        fclose(fp);
        free(selected);
        free(full_path);
        return;
    }

    cJSON_ReplaceItemInObject(param, "loading", cJSON_CreateString(name)); // change la valeur de loading

    fp = fopen(params, "w");
    if (!fp) {
        printf("Error: cannot write in the parameter file\n");
        cJSON_Delete(param);    // Si on peut pas ecrire dans le fichier params 
        free(selected);
        free(full_path);
        return;
    }

    char *param_str = cJSON_Print(param);   // formate le texte et mets dans params.json
    fputs(param_str, fp);
    file = full_path;

    cJSON_free(param_str);
    fclose(fp);
    cJSON_Delete(param);    //rend toute la mémoire utilisé
    free(selected);
    free(full_path);

}

// update le contenue du fichier choisie avec la valeur actuelle de ``data``
void update(char *filename) {
    char *json_str = cJSON_Print(data);
    if (!json_str) return;

    FILE *f = fopen(filename, "w");
    if (!f) {
        filename[strlen(filename - 5)] = '\0';
        printf("Error: Couldn't save %s\n", filename);  // Si n'a pas pu ouvrir le fichier
        cJSON_free(json_str);
        return;
    }

    fputs(json_str, f);

    fclose(f);
    cJSON_free(json_str);   //libere mémoire
}

// gere la partie input user et appelle les fonction correspondante
void run(int running) {
    char user_input[100];

    while (running > 0) {
        printf("> ");
        fgets(user_input, sizeof(user_input), stdin);
        
        user_input [strcspn(user_input, "\n")] = '\0';

        if (strlen(user_input) <= 0) {
            continue;
        }

        if (strcmp(user_input, "quit") == 0 || strcmp(user_input, "q") == 0) {
            running = 0;
            printf("Saving and exiting the programm.");
            update(file);

        } else if (strcmp(user_input, "switch") == 0) {
            int count;

            char **files = path_listdir(path, &count);

            if (files) {
                for (int i = 0; i < count; i++) {
                    printf("%s\n", files[i]);
                }
            }

            
            for (int i = 0; i < count; i++) {
                free(files[i]);
            }   // Libération de la mémoire

            free(files);
        }

    }
}

// initialisation des variables et creation des fichiers si inexistants
int main() {
    char *appdata = getenv("APPDATA");

    if (!appdata) {
        appdata = getenv("HOME");
    }

    path = path_join(appdata, "Inventory App");
    file = path_join(path, "default.json");
    params = path_join(path, "params.json");

    printf("path : %s\nfile : %a\nparams : %f\n", path, file, params);

    #ifdef _WIN32 
        _mkdir(path);
    #else
        mkdir(path, 0755);
    #endif

    FILE *f = fopen(file, "r");

    if (!f) {

        data = cJSON_CreateArray();

        char *text = cJSON_Print(data);

        f = fopen(file, "w");

        if (f) {
            fputs(text, f);
            fclose(f);
        }

        cJSON_free(text);

    } else {

        char buffer[1024];

        size_t len = fread(buffer, 1, sizeof(buffer) - 1, f);
        buffer[len] = '\0';

        data = cJSON_Parse(buffer);

        fclose(f);
    }

    if (!data) {
        printf("Error: invalid Inventory file\n");
        data = cJSON_CreateArray();
    }

    FILE *f = fopen(params, "r");

    if (!f) {
        cJSON *obj = cJSON_CreateObject();

        cJSON_AddStringToObject(obj, "loading", "default.json");

        char *text = cJSON_Print(obj);

        f = fopen(params, "w");   // creer params.json et mets dedans le parametre par default

        fputs(text, f);

        fclose(f);

        free(text);
        cJSON_Delete(obj);

    } else {
        char buffer[1024];
        int len = fread(buffer, 1, sizeof(buffer), f);
        fclose(f);

        cJSON *json = cJSON_Parse(buffer);
        if (!json) {
            const char *error_ptr = cJSON_GetErrorPtr();
            if (error_ptr != NULL) {
                printf("Error: %s\n", error_ptr);
            }
            cJSON_Delete(json);
        }

        cJSON *filename = cJSON_GetObjectItemCaseSensitive(json, "loading");
        if (cJSON_IsString(filename) && (filename->valuestring != NULL)) {
            file = path_join(path, filename->valuestring);
        }
        cJSON_Delete(json);
    }

    char filename[] = path_filename(file);
    size_t len = strlen(filename);
    if (len >= 5) {
        filename[len - 5] = '\0';
    } 

    printf("Welcome to the inventory managment program.\nType 'help' for a list of command.\nCurrent inventory file : %s\n", filename);
    run(1);

    return 0;
}