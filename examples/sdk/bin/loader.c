/**
 * Framework Loader
 * =================
 * Optional managed launcher for framework applications.
 * This loader can be specified in the NAK's loader configuration
 * to wrap application execution with framework initialization.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void print_usage(const char* prog) {
    fprintf(stderr, "Usage: %s --app <path> [args...]\n", prog);
    fprintf(stderr, "\nFramework Loader - Wraps app execution with framework init\n");
}

int main(int argc, char* argv[]) {
    const char* app_path = NULL;
    int app_argc = 0;
    char** app_argv = NULL;
    
    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--app") == 0 && i + 1 < argc) {
            app_path = argv[++i];
            /* Remaining args are for the app */
            app_argc = argc - i - 1;
            app_argv = &argv[i + 1];
            break;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    if (!app_path) {
        print_usage(argv[0]);
        return 1;
    }
    
    printf("[loader] Framework Loader starting\n");
    printf("[loader] App: %s\n", app_path);
    printf("[loader] NAH_APP_ID: %s\n", getenv("NAH_APP_ID") ?: "(not set)");
    printf("[loader] NAH_APP_ROOT: %s\n", getenv("NAH_APP_ROOT") ?: "(not set)");
    
    /* Execute the app */
    printf("[loader] Executing application...\n\n");
    
    /* Build argv for execv */
    char** new_argv = malloc((app_argc + 2) * sizeof(char*));
    new_argv[0] = (char*)app_path;
    for (int i = 0; i < app_argc; i++) {
        new_argv[i + 1] = app_argv[i];
    }
    new_argv[app_argc + 1] = NULL;
    
    execv(app_path, new_argv);
    
    /* If we get here, exec failed */
    perror("[loader] exec failed");
    free(new_argv);
    return 1;
}
