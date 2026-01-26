// Document Viewer Component
// Read-only document viewing

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
    const char* component_id = getenv("NAH_COMPONENT_ID");
    const char* component_uri = getenv("NAH_COMPONENT_URI");
    const char* component_referrer = getenv("NAH_COMPONENT_REFERRER");
    
    printf("===========================================\n");
    printf("  Document Viewer (Read-Only)\n");
    printf("===========================================\n");
    printf("\n");
    
    if (component_id) {
        printf("Component: %s\n", component_id);
        if (component_referrer) {
            printf("Called from: %s\n", component_referrer);
        }
        printf("\n");
    }
    
    if (argc > 1) {
        printf("Viewing file: %s\n", argv[1]);
        printf("(Read-only mode - use editor to modify)\n");
    } else {
        printf("Viewer ready. No file specified.\n");
    }
    
    printf("\nLaunch editor with:\n");
    printf("  nah launch com.example.suite://editor?file=document.txt\n");
    printf("\n");
    
    return 0;
}
