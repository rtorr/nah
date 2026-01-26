// Document Editor Component
// Demonstrates component-specific environment variables

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
    const char* component_id = getenv("NAH_COMPONENT_ID");
    const char* component_uri = getenv("NAH_COMPONENT_URI");
    const char* component_path = getenv("NAH_COMPONENT_PATH");
    const char* component_query = getenv("NAH_COMPONENT_QUERY");
    const char* component_fragment = getenv("NAH_COMPONENT_FRAGMENT");
    const char* component_referrer = getenv("NAH_COMPONENT_REFERRER");
    
    printf("===========================================\n");
    printf("  Document Editor\n");
    printf("===========================================\n");
    printf("\n");
    
    if (component_id) {
        printf("Component Info:\n");
        printf("  ID:        %s\n", component_id);
        if (component_uri) printf("  URI:       %s\n", component_uri);
        if (component_path) printf("  Path:      %s\n", component_path);
        if (component_query) printf("  Query:     %s\n", component_query);
        if (component_fragment) printf("  Fragment:  %s\n", component_fragment);
        if (component_referrer) printf("  Referrer:  %s\n", component_referrer);
        printf("\n");
    }
    
    if (argc > 1) {
        printf("Opening file: %s\n", argv[1]);
    } else if (component_query) {
        // Parse query string for file parameter
        char query_copy[1024];
        strncpy(query_copy, component_query, sizeof(query_copy) - 1);
        query_copy[sizeof(query_copy) - 1] = '\0';
        
        char* token = strtok(query_copy, "&");
        while (token != NULL) {
            if (strncmp(token, "file=", 5) == 0) {
                printf("Opening file from URI: %s\n", token + 5);
                break;
            }
            token = strtok(NULL, "&");
        }
    } else {
        printf("Editor ready. No file specified.\n");
    }
    
    printf("\nLaunch viewer with:\n");
    printf("  nah launch com.example.suite://viewer\n");
    printf("\n");
    
    return 0;
}
