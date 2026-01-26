// Suite Launcher - Main entry point
// Shows menu to launch individual components

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
    printf("===========================================\n");
    printf("  Document Suite v1.0.0\n");
    printf("===========================================\n");
    printf("\n");
    printf("Available components:\n");
    printf("  1. Editor   - Edit documents\n");
    printf("  2. Viewer   - View documents\n");
    printf("  3. Converter - Convert formats (hidden)\n");
    printf("\n");
    printf("Launch components directly:\n");
    printf("  nah launch com.example.suite://editor\n");
    printf("  nah launch com.example.suite://viewer\n");
    printf("  nah launch com.example.suite://converter\n");
    printf("\n");
    
    return 0;
}
