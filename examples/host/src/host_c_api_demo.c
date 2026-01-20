/**
 * Host C API Demo
 * ================
 * Demonstrates the NAH C API for host integration.
 * 
 * This example shows how to use the stable C ABI to:
 * - Create a NAH host instance
 * - List installed applications
 * - Get a launch contract
 * - Access contract fields for launching an app
 *
 * Build:
 *   cc -o host_c_api_demo host_c_api_demo.c -L/path/to/lib -lnahhost -lstdc++
 *
 * Or link against the shared library:
 *   cc -o host_c_api_demo host_c_api_demo.c -L/path/to/lib -lnahhost
 */

#include <nah/nah.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_separator(void) {
    printf("------------------------------------------------------------\n");
}

int main(int argc, char* argv[]) {
    const char* nah_root = "/nah";
    int32_t i;
    
    if (argc > 1) {
        nah_root = argv[1];
    }
    
    printf("NAH C API Demo\n");
    printf("==============\n\n");
    
    /* Check ABI version compatibility */
    if (nah_abi_version() != NAH_ABI_VERSION) {
        fprintf(stderr, "ABI version mismatch: header=%d, library=%d\n",
                NAH_ABI_VERSION, nah_abi_version());
        return 1;
    }
    
    printf("Library version: %s\n", nah_version_string());
    printf("ABI version: %d\n", nah_abi_version());
    printf("NAH Root: %s\n\n", nah_root);
    
    /* Create host instance */
    NahHost* host = nah_host_create(nah_root);
    if (!host) {
        fprintf(stderr, "Error: %s\n", nah_get_last_error());
        return 1;
    }
    
    /* List installed applications */
    print_separator();
    printf("Installed Applications:\n");
    print_separator();
    
    NahAppList* apps = nah_host_list_apps(host);
    if (!apps) {
        fprintf(stderr, "Error listing apps: %s\n", nah_get_last_error());
        nah_host_destroy(host);
        return 1;
    }
    
    int32_t app_count = nah_app_list_count(apps);
    if (app_count == 0) {
        printf("  (no applications installed)\n");
    } else {
        for (i = 0; i < app_count; i++) {
            printf("  %s@%s\n", 
                   nah_app_list_id(apps, i),
                   nah_app_list_version(apps, i));
        }
    }
    printf("\n");
    
    /* List profiles */
    print_separator();
    printf("Available Profiles:\n");
    print_separator();
    
    NahStringList* profiles = nah_host_list_profiles(host);
    if (profiles) {
        int32_t profile_count = nah_string_list_count(profiles);
        if (profile_count == 0) {
            printf("  (no profiles found)\n");
        } else {
            for (i = 0; i < profile_count; i++) {
                printf("  %s\n", nah_string_list_get(profiles, i));
            }
        }
        nah_string_list_destroy(profiles);
    }
    printf("\n");
    
    /* Get launch contract for first app */
    if (app_count > 0) {
        const char* app_id = nah_app_list_id(apps, 0);
        const char* app_version = nah_app_list_version(apps, 0);
        
        print_separator();
        printf("Launch Contract for %s:\n", app_id);
        print_separator();
        
        NahContract* contract = nah_host_get_contract(host, app_id, app_version, NULL);
        if (!contract) {
            fprintf(stderr, "Error: %s\n", nah_get_last_error());
        } else {
            printf("  App: %s v%s\n", 
                   nah_contract_app_id(contract),
                   nah_contract_app_version(contract));
            printf("  NAK: %s v%s\n",
                   nah_contract_nak_id(contract),
                   nah_contract_nak_version(contract));
            printf("  Binary: %s\n", nah_contract_binary(contract));
            printf("  CWD: %s\n", nah_contract_cwd(contract));
            
            /* Library paths */
            int32_t lib_count = nah_contract_library_path_count(contract);
            if (lib_count > 0) {
                printf("  Library Paths (%s):\n", 
                       nah_contract_library_path_env_key(contract));
                for (i = 0; i < lib_count; i++) {
                    printf("    %s\n", nah_contract_library_path(contract, i));
                }
            }
            
            /* Arguments */
            int32_t arg_count = nah_contract_argc(contract);
            if (arg_count > 0) {
                printf("  Arguments:\n");
                for (i = 0; i < arg_count; i++) {
                    printf("    [%d] %s\n", i, nah_contract_argv(contract, i));
                }
            }
            
            /* Warnings */
            int32_t warn_count = nah_contract_warning_count(contract);
            if (warn_count > 0) {
                printf("  Warnings: %d\n", warn_count);
                for (i = 0; i < warn_count; i++) {
                    printf("    - %s\n", nah_contract_warning_key(contract, i));
                }
            }
            
            /* Environment (as JSON) */
            printf("\n  Environment (JSON):\n");
            char* env_json = nah_contract_environment_json(contract);
            if (env_json) {
                /* Truncate if too long for display */
                if (strlen(env_json) > 200) {
                    env_json[197] = '.';
                    env_json[198] = '.';
                    env_json[199] = '.';
                    env_json[200] = '\0';
                }
                printf("    %s\n", env_json);
                nah_free_string(env_json);
            }
            
            nah_contract_destroy(contract);
        }
    }
    
    nah_app_list_destroy(apps);
    
    printf("\n");
    print_separator();
    printf("Demo complete.\n");
    
    nah_host_destroy(host);
    return 0;
}
