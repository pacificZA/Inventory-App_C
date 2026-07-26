#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
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

// ajoute si besoin .json a la fin d'une chaine
char* jsonify(const char *name) {
    size_t len = strlen(name);

    if(len >= 5 && strcmp(name + len - 5, ".json") == 0) {
        return name;
    } 

    char *selected = malloc(len + 6);
    sprintf(selected, "%s.json", name);
    return selected;
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

// transforme en minuscule une chaine
char* lower(char *str) {
    size_t len = strlen(str);
    char *str1 = calloc(len + 1, sizeof(char));

    for (size_t i = 0; i < len; i++) {
        str1[i] = tolower((unsigned char)str[i]);
    }
    return str1;
}

// trouve un item par son nom
cJSON* find_item(const char *name) {
    if (!name) return

    name = lower(name);

    cJSON *item = NULL;
    cJSON_ArrayForEach(item, data) {
        cJSON *found = cJSON_GetObjectItem(item, "name");
        if (cJSON_IsString(found) && strcmp(name, found) == 0) {
            free(name);
            return item;
        }
    }
    free(name);
    return NULL;
}

// cherche si dans tags il y a search
int has_tag(const char *tags, const char *search) {
    char *start = tags;
    char *end;
    size_t search_len = strlen(search);

    while (*start) {
        end = strchr(start, '/');

        if (!end) end = start + strlen(start);

        size_t len = end - start;

        if (len == search_len && strncmp(start, search, len) == 0) return 1;

        if (*end == '\0') break;
        start = end + 1;
    }

    return 0;
}

// ajoute un item a l'inventaire actuel
void add_item(char *item, char *tags, float count) {

    if (!item) {
        printf("Error: Invalid item data. Please provide 3 parameters: name, tags, quantity).\n");
        return;
    }
    item = lower(item);

    if (find_item(item)) {
        printf("Error: Item that name already exists.\n");
        free(item);
        return;
    }

    const char *reserved[] = {
    "quit", "q",
    "delete", "d",
    "+", "-",
    "new", "n",
    "edit", "e",
    "remove", "r",
    "help", "h",
    "all", "a",
    "switch", "s",
    "current", "c",
    "file", "f"
    };

    for (size_t i = 0; i < sizeof(reserved) / sizeof(reserved[0]); i++) {
        if (strcmp(item, reserved[i]) == 0) {
            printf("Error: Can't name an item a command name (use help to see).\n");
            return;
        }
    }

    if (count < 0) {
        printf("Error: Invalid quantity: %g. Can't put negative quantity.\n", count);
        free(item);
        return;
    }

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "name", item);
    cJSON_AddStringToObject(obj, "tags", tags);
    cJSON_AddNumberToObject(obj, "quantity", count);
    cJSON_AddItemToArray(data, obj);

    update(file);
    free(item);
}

/* Verifie la validité d'un nom de fichier! ne contient pas ``/\:*"<>|`` ou n'est pas vide 
renvoie 0 si c'est bon sinon 1*/
int check_name(const char *filename) {
    if (!filename || filename[0] == '\0') {
        printf("Error: File name must be at least one character long");
        return 1;
    }
    const char prob[] = "/\\:*\"<>|";
    size_t size = strlen(filename);

    if (strcspn(filename, prob) == size) { // strcspn donne la longeur avant de renconter un character du deuxieme arg 
        return 0;                          // donc si = -> pas de char en commun
    }
    printf("Error: Invalid file name, can't contain: /\\:*\"<>|.\n");
    return 1;
}

// change de fichier d'inventaire et modifie params.json
void switch_file(const char *name) {
    if (check_name(name) == 1) return;

    char *selected = jsonify(name);

    if (strcmp(selected, "params.json") == 0) {
        printf("Error: can't switch to the parameter file\n"); // Si le nom est params.json -> refuse
        free(selected);
        return;
    }

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
    size_t len = fread(buffer, 1, sizeof(buffer)-1, f);    //creer un buffer qui stock l'interieur du fichier avant de le parser
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
    update(file);

    cJSON_free(param_str);
    fclose(fp);
    cJSON_Delete(param);    //rend toute la mémoire utilisé
    free(selected);
    free(full_path);

}

// supprime le fichier selectionné et switch si c'est l'actuel
void delete_file(const char *name) {
    if (check_name(name) == 1) return;

    char* selected = jsonify(name);
    char* selected_path = path_join(path, selected);

    printf("are you sure you want to delete the inventory file? This action cannot be undone. (y/n)\n");

    char *confirm;
    fgets(confirm, sizeof(confirm), stdin);
    confirm [strcspn(confirm, "\n")] = '\0';
    confirm = lower(confirm);

    if (strcmp(confirm, "y") != 0) {
        printf("Deletion cancelled.\n");
        free(selected_path);
        free(selected);
        free(confirm);
        return;
    }
    free(confirm);

    if (strcmp(selected, "default.json") == 0 || strcmp(selected, "params.json" == 0)) {
        printf("Error: Can't delete the default/params file; try another one.\n");
        free(selected);
        free(selected_path);
        return;
    }

    if (strcmp(selected_path, file) == 0) {
        switch_file("default");

        int rmv = remove(selected_path);
        if (rmv) {
            printf("Error: Deletion failed (check your permissions).\n");
            free(selected_path);
            free(selected);
            return;
        }

        printf("Current inventory file deleted. Switching to default.\n");
        free(selected_path);
        free(selected);
        return;
    }

    int rmv = remove(selected_path);
    if (rmv) {
        printf("Error: Deletion failed, file might not exists (check your permissions).\n");
        free(selected_path);
        free(selected);
        return;
    }
    printf("Inventory file deleted.\n");
    free(selected_path);
    free(selected);
}

// creer un fichier json avec le nom donné
void create_file(const char *name) {
    if (check_name(name) == 1) return;

    char *selected = jsonify(name);

    char *file_path = path_join(path, selected);
    
    FILE *f = fopen(file_path, "r");
    if (f) {
        printf("Error: File that name already exists.\n");
        free(selected);
        fclose(f);
        return;
    }

    printf("Creating new inventory file %s...\n", name);

    cJSON *contain = cJSON_CreateArray();
    char *text = cJSON_Print(contain);

    f = fopen(file_path, "w");
    if (f) {
        fputs(text, f);
        fclose(f);
        printf("File succesfully created.\n");
    } else {
        printf("Error: The file couldn't be created.\n");
    }
    cJSON_free(text);
    cJSON_Delete(contain);

    free(selected);
}

// gere la partie input user et appelle les fonction correspondante
void run(int running) {
    char user_input[1024];

    while (running > 0) {
        printf("> ");
        fgets(user_input, sizeof(user_input), stdin);
        
        user_input[strcspn(user_input, "\n")] = '\0'; // rempalce le dernier character (\n) par \0
        printf("\n");

        if (strlen(user_input) <= 0) {
            continue;
        }

        if (strcmp(user_input, "quit") == 0 || strcmp(user_input, "q") == 0) {
            running = 0;
            printf("Saving and exiting the programm.");
            update(file);

        } else if (strcmp(user_input, "delete") == 0) {
            int count;

            char **files = path_listdir(path, &count);

            if (files) {

                if (count < 2) {
                    printf("Error: No file can currently be deleted (Not enough files available).\n");
                    continue;
                }
                int names = 0;

                for (int i = 0; i < count; i++) {
                    char *name = files[i];
                    size_t len = strlen(name);
                    if ((len >= 5 && strcmp(name + len - 5, ".json") == 0) && (strcmp(name, "params.json") != 0 || strcmp(name, "default.json") != 0)) {
                        printf("%s  ", name[len - 5]);
                        names++;
                        if (names == 4) {
                            printf("\n");
                            names = 0;
                        }
                    }
                }
            }
            
            for (int i = 0; i < count; i++) {
                free(files[i]);
            }   // Libération de la mémoire

            free(files);

            char name[256];
            printf("\n>Enter the name of the inventory file to delete: ");
            fgets(name, sizeof(name), stdin);
            name[strcspn(name, "\n")] = '\0';
            print("\n");
            delete_file(name);

        } else if (user_input[0] == "d" && user_input[1] == " ") {
            char *input = user_input + 2;

            if (strcmp(input, "all") == 0 || strcmp(input, "a") == 0) {
                char confirm[256];
                printf("are you sure you want to delete every app file? This action cannot be undone. (y/n)\n");
                fgets(confirm, sizeof(confirm), stdin);
                confirm[strcspn(confirm, "\n")] = '\0';
                *confirm = lower(confirm);

                if (confirm[0] != "y") {
                    free(confirm);
                    printf("Deletion canceled.\n");
                    continue;
                }
                
                confirm[256];
                printf("are you really sure ? This action cannot be undone. (y/n)\n");
                fgets(confirm, sizeof(confirm), stdin);
                *confirm = lower(confirm);

                if (confirm[0] != "y") {
                    free(confirm);
                    printf("Deletion canceled.\n");
                    continue;
                }
                free(confirm);

                int count;

                char **files = path_listdir(path, &count);

                if (files) {
                    for (int i = 0; i < count; i++) {
                        int rmv = remove(files[i]);
                        if (rmv) {
                            printf("Error: Deletion of %s failed (check your permissions)", files[i]);
                        }
                    }
                    printf(">Files deleted, exiting the app...");
                    running = 0;
                }
                
                for (int i = 0; i < count; i++) {
                    free(files[i]);
                }   // Libération de la mémoire

                free(files);

            } else if (strlen(input) > 0) delete_file(input);

        } else if (user_input[0] == "+" || user_input[0] == "-") {
            char *item;
            float value = strtof(user_input, &item); // str to f (float) pas str of ou quoi

            if (item == user_input || !isspace((unsigned char)*item)) {
                // Aucun nombre n'a été lu / format pas bon +1item ou +1 item item
                printf("Error: Invalid command format. Please use %s<number> <item_name>.\n", user_input[0]);
                continue;
            }

            while (isspace((unsigned char)*item)) {
                item++; // enleve les espaces avant le nom de l'item
            }

            cJSON *obj = find_item(item);
            if (!obj) {
                printf("Error: item '%s' not found.\n", item);
                continue;
            }

            cJSON *quantity = cJSON_GetObjectItem(obj, "quantity");
            if (cJSON_IsNumber(quantity)) {
                double new_quantity = quantity->valuedouble;
                new_quantity += value;
                if (new_quantity >= 0) {
                    cJSON_SetNumberValue(obj, new_quantity);
                    update(file);
                    printf("Item '%s' updated. New quantity: %f.\n", item, new_quantity);
                } else {
                    printf("Error: Quantity cannot be negative.\n");
                }
            }

        } else if (strcmp(user_input, "new") == 0) {
            printf(">Enter the item name: ");
            char name[256];
            fgets(name, sizeof(name), stdin);
            name[strcspn(name, "\n")] = '\0';
            
            if (find_item(name)) {
                printf("Item that name already exists.\n");
                continue;
            }

            char *tags;
            printf("\n>Enter the item tags (slash separated): ");
            fgets(tags, sizeof(tags), stdin);

            float quantity;
            printf("\n>Enter the item quantity: ");
            fgets(tags, sizeof(tags), stdin);

            printf("\n");
            add_item(name, tags, quantity);
            

        } else if (user_input[0] == "n" && user_input[1] == " ") {
            char *command = user_input + 2; // retire les 2 premiers character "n "

            while (isspace((unsigned char)*command)) command ++; // retire les espace eventuel avant

            // retire les espaces a la fin
            char *end = command + strlen(command) - 1;
            while (end > command && isspace((unsigned char)*end)) *end-- = '\0';
            
            char *args[3];
            int count = 0;

            char *token = strtok(command, ",");

            while (token && count < 3) {

                // enleve les espaces autour
                while (isspace((unsigned char)*token)) token++;

                char *end = token + strlen(token) - 1;
                while (end > token && isspace((unsigned char)*end)) *end-- = '\0';

                args[count++] = token;

                token = strtok(NULL, ",");
            }

            if (count != 3 || token != NULL) {
                printf("Error: Invalid command format. Please use the format: n <item_name>,<tags>,<quantity>.\n");
                continue;
            }

            char *end;
            float quantity = strtof(args[2], &end);

            if (*end != '\0') {
                printf("Error: Invalid quantity.\n");
                continue;
            }

            add_item(args[0], args[1], quantity);

        } else if (strcmp(user_input, "edit") == 0) {
            char name[256];
            char new_tags[256];

            printf(">Enter the item name to edit: ");
            fgets(name, sizeof(name), stdin);
            name[strcspn(name, "\n")] = '\0';

            cJSON *obj = find_item(name);

            if (!obj) {
                printf("Error: Item '%s' not found.\n", name);
                continue;
            }

            printf(">Enter the new item tags (slash-separated): ");
            fgets(new_tags, sizeof(new_tags), stdin);
            new_tags[strcspn(new_tags, "\n")] = '\0';

            printf("\n");

            char *tags = lower(new_tags);

            cJSON_ReplaceItemInObject(obj, "tags", cJSON_CreateString(tags));
            free(tags);

            update(file);

        } else if (user_input[0] == "e" && user_input[1] == " ") {
            char *command = user_input + 2;

            while (isspace((unsigned char)*command)) command ++; // retire les espace eventuel avant

            char *end = command + strlen(command) - 1;
            while (end > command && isspace((unsigned char)*end)) *end-- = '\0';

            char *args[2];
            int count = 0;
            char *token = strtok(command, ",");

            while (token && count < 2) {
                while (isspace((unsigned char)*token)) token++;

                char *end = token + strlen(token) - 1;
                while (end > token && isspace((unsigned char)*end)) *end-- = '\0';

                args[count++] = token;

                token = strtok(NULL, ",");
            }

            if (count != 2 || token != NULL) {
                printf("Error: Invalid command format. Please use the format: n <item_name>,<tags>.\n");
                continue;
            }

            cJSON *obj = find_item(args[0]);

            if (!obj) {
                printf("Error: Item '%s' not found.\n", args[0]);
                continue;
            }

            char *tags = lower(args[1]);

            cJSON_ReplaceItemInObject(obj, "tags", cJSON_CreateString(tags));
            free(tags),

            update(file);

        } else if (strcmp(user_input, "remove") == 0) {
            char name[256];
            printf(">Enter the item name to remove: ");
            fgets(name, sizeof(name), stdin);

            name[strcspn(name, "\n")] = '\0';
            printf("\n");

            cJSON *obj = find_item(name);

            if (!obj) {
                printf("Error: Item '%s' not found.\n", name);
                continue;
            }

            cJSON *item = NULL;
            int count = 0;
            cJSON_ArrayForEach(item, data) {
                cJSON *found = cJSON_GetObjectItem(item, "name");
                if (cJSON_IsString(found) && found == cJSON_GetObjectItem(obj, "name")) {
                    break;
                }
                count++;
            }

            cJSON_DeleteItemFromArray(data, count);

        } else if (user_input[0] == "r" && user_input[1] == " ") {
            char *command = user_input + 2;

            while (isspace((unsigned char)*command)) command ++; // retire les espace eventuel avant

            char *end = command + strlen(command) - 1;
            while (end > command && isspace((unsigned char)*end)) *end-- = '\0';

            if (!command) {
                printf("Error: Invalid command format. Please use the format: r <item_name>.\n");
                continue;
            }

            cJSON *obj = find_item(command);

            if (!obj) {
                printf("Error: Item '%s' not found.\n", command);
                continue;
            }

            cJSON *item = NULL;
            int count = 0;
            cJSON_ArrayForEach(item, data) {
                cJSON *found = cJSON_GetObjectItem(item, "name");
                if (cJSON_IsString(found) && found == cJSON_GetObjectItem(obj, "name")) {
                    break;
                }
                count++;
            }

            cJSON_DeleteItemFromArray(data, count);

        } else if (user_input[0] == "h" || strcmp(user_input, "help") == 0) {
            printf("Help:\n");
            printf("  help - Show this help message *\n");
            printf("  new - Add a new item *\n");
            printf("  edit - Edit an existing item tags *\n");
            printf("  remove - Remove an existing item *\n");
            printf("  quit - Quit the program\n");
            printf("  +<number> <item_name> - Increase the quantity of an item\n");
            printf("  -<number> <item_name> - Decrease the quantity of an item\n");
            printf("  <item_name/tags> - Search for an item by name or tag\n");
            printf("  all - Show all items\n");
            printf("  current - Show the current inventory file\n");
            printf("  switch - Switch to another inventory file *\n");
            printf("  file - Create a new inventory file *\n");
            printf("  delete - Delete an inventory file (cannot be undone), the 'all' argument delete every app files *\n");
            printf("  all command with an asterisk (*) can be used with the first letter only using a comma-separated list of values instead of interactive input, for example: n <item_name>, <tags>, <quantity>\n");


        } else if (user_input[0] == "a" || strcmp(user_input, "all") == 0) {
            cJSON *item = NULL;

            cJSON_ArrayForEach(item, data) {
                cJSON *name = cJSON_GetObjectItem(item, "name");
                cJSON *tags = cJSON_GetObjectItem(item, "tags");
                cJSON *quantity = cJSON_GetObjectItem(item, "quantity");

                printf("Item: %s, Tags: %s, Quantity: %d\n", name->valuestring, tags->valuestring, quantity->valueint);
            }

        } else if (strcmp(user_input, "switch") == 0) {
            int count;

            char **files = path_listdir(path, &count);

            if (files) {
                int names = 0;

                for (int i = 0; i < count; i++) {
                    char *name = files[i];
                    size_t len = strlen(name);
                    if ((len >= 5 && strcmp(name + len - 5, ".json") == 0) && (strcmp(name, "params.json") != 0 || strcmp(name, "default.json") != 0)) {
                        printf("%s  ", name[len - 5]);
                        names++;
                        if (names == 4) {
                            printf("\n");
                            names = 0;
                        }
                    }
                }
            }
            
            for (int i = 0; i < count; i++) {
                free(files[i]);
            }   // Libération de la mémoire
            free(files);

            char name[256];
            printf(">Enter the name of the inventory file to switch to: ");

            fgets(name, sizeof(name), stdin);
            name[strcspn(name, "\n")] = '\0';
            printf("\n");

            switch_file(name);

        } else if (user_input[0] = "s" && user_input[1] == " ") {
            char *command = user_input + 2;

            while (isspace((unsigned char)*command)) command ++; // retire les espace eventuel avant

            char *end = command + strlen(command) - 1;
            while (end > command && isspace((unsigned char)*end)) *end-- = '\0';

            if (!command) {
                printf("Error: Invalid command format. Please use the format: s <file_name>.\n");
                continue;
            }

            switch_file(command);

        } else if (user_input[0] == "c" || strcmp(user_input, "current") == 0) {
            char *filename = path_filename(file);
            size_t len = strlen(filename);

            printf("Current inventory file: %s", filename[len - 5]);
            
        } else if (strcmp(user_input, "file") == 0) {
            char name[256];
            printf(">Enter the new file name: ");

            fgets(name, sizeof(name), stdin);
            name[strcspn(name, "\n")] = '\0';
            printf("\n");

            create_file(name);
        } else if (user_input[0] == "f" && user_input[1] == " ") {
            char *command = user_input + 2;

            while (isspace((unsigned char)*command)) command ++; // retire les espace eventuel avant

            char *end = command + strlen(command) - 1;
            while (end > command && isspace((unsigned char)*end)) *end-- = '\0';

            if (!command) {
                printf("Error: Invalid command format. Please use the format: f <file_name>.\n");
                continue;
            }

            create_file(command);
        } else {
            cJSON *item = find_item(user_input);

            if (item) {
                cJSON *name = cJSON_GetObjectItem(item, "name");
                cJSON *tags = cJSON_GetObjectItem(item, "tags");
                cJSON *quantity = cJSON_GetObjectItem(item, "quantity");

                printf("Item: %s, Tags: %s, Quantity: %d\n", name->valuestring, tags->valuestring, quantity->valueint);
                continue;
            }

            int count = 0;
            char *tags = lower(user_input);

            item = NULL;

            cJSON_ArrayForEach(item, data) {
                cJSON *name = cJSON_GetObjectItem(item, "name");
                cJSON *tags = cJSON_GetObjectItem(item, "tags");
                cJSON *quantity = cJSON_GetObjectItem(item, "quantity");

                if (cJSON_IsString(tags) && cJSON_IsString(name) && cJSON_IsNumber(quantity)) {
                    if (has_tag(tags->valuestring, user_input)) {
                        printf("Item: %s, Tags: %s, Quantity: %d\n", name->valuestring, tags->valuestring, quantity->valueint);
                        count++;
                    }
                }
            }
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

    printf("path : %s\nfile : %s\nparams : %s\n", path, file, params);

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
        printf("Error: invalid Inventory file, starting with an empty one.\n");
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

    char *filename = path_filename(file);
    size_t len = strlen(filename);
    if (len >= 5) {
        filename[len - 5] = '\0';
    } 

    printf("Welcome to the inventory managment program.\nType 'help' for a list of command.\nCurrent inventory file : %s\n", filename);
    run(1);

    return 0;
}