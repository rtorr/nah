// Format Converter Component
// Hidden component - not shown in launcher UI

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    const char* component_id = getenv("NAH_COMPONENT_ID");
    
    printf("===========================================\n");
    printf("  Format Converter\n");
    printf("===========================================\n");
    printf("\n");
    
    if (component_id) {
        printf("Component: %s (hidden component)\n", component_id);
        printf("\n");
    }
    
    if (argc < 3) {
        printf("Usage: converter <input> <output>\n");
        printf("\n");
        printf("This is a hidden component typically called by\n");
        printf("other components, not directly by users.\n");
        return 1;
    }
    
    printf("Converting: %s -> %s\n", argv[1], argv[2]);
    printf("Conversion complete!\n");
    printf("\n");
    
    return 0;
}
