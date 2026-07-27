#ifndef PATH
    #define PATH

    char* path_join(const char* base, const char* sub);
    const char* path_filename(char* path);
    void path_normalize(char* path);
    char** path_listdir(const char *path, int *count);

#endif