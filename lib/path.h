#ifndef PATH
    #define PATH

    char* path_join(const char* base, const char* sub);
    const char* path_filename(const char* path);
    void path_normalize(char* path);

#endif