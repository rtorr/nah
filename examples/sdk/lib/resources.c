#include "framework/framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* load_file(const char* path, size_t* size) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* data = malloc(len + 1);
    if (!data) {
        fclose(f);
        return NULL;
    }
    
    size_t read = fread(data, 1, len, f);
    fclose(f);
    
    data[read] = '\0';
    if (size) *size = read;
    
    return data;
}

char* framework_load_sdk_resource(framework_ctx_t* ctx, const char* name, size_t* size) {
    const char* sdk_root = framework_get_sdk_root(ctx);
    if (!sdk_root || !name) return NULL;
    
    char path[4096];
    snprintf(path, sizeof(path), "%s/resources/%s", sdk_root, name);
    
    return load_file(path, size);
}

char* framework_load_app_resource(framework_ctx_t* ctx, const char* name, size_t* size) {
    const char* app_root = framework_get_app_root(ctx);
    if (!app_root || !name) return NULL;
    
    char path[4096];
    snprintf(path, sizeof(path), "%s/assets/%s", app_root, name);
    
    return load_file(path, size);
}
