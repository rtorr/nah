#ifndef FRAMEWORK_H
#define FRAMEWORK_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Framework Context
 * ============================================================================ */

typedef struct framework_ctx framework_ctx_t;

typedef struct {
    const char* app_id;      /* NULL to read from NAH_APP_ID */
    const char* app_root;    /* NULL to read from NAH_APP_ROOT */
    int log_level;
    bool use_nah_env;        /* Read paths from NAH environment */
} fw_init_options_t;

/* Initialize framework */
framework_ctx_t* framework_init(const fw_init_options_t* opts);

/* Shutdown framework */
void framework_shutdown(framework_ctx_t* ctx);

/* Check if running under NAH management */
bool framework_is_nah_managed(void);

/* ============================================================================
 * Application Info
 * ============================================================================ */

const char* framework_get_app_id(framework_ctx_t* ctx);
const char* framework_get_app_version(framework_ctx_t* ctx);
const char* framework_get_app_root(framework_ctx_t* ctx);
const char* framework_get_sdk_root(framework_ctx_t* ctx);

/* ============================================================================
 * Resource Loading
 * ============================================================================ */

/* Load resource from SDK resources directory */
char* framework_load_sdk_resource(framework_ctx_t* ctx, const char* name, size_t* size);

/* Load resource from app assets directory */
char* framework_load_app_resource(framework_ctx_t* ctx, const char* name, size_t* size);

/* ============================================================================
 * Lifecycle Management
 * ============================================================================ */

typedef struct {
    int (*on_start)(framework_ctx_t* ctx, void* user_data);
    void (*on_stop)(framework_ctx_t* ctx, void* user_data);
    void (*on_config_reload)(framework_ctx_t* ctx, void* user_data);
} fw_lifecycle_callbacks_t;

/* Run application with lifecycle callbacks */
int framework_run(framework_ctx_t* ctx, const fw_lifecycle_callbacks_t* callbacks, void* user_data);

/* ============================================================================
 * Logging
 * ============================================================================ */

#define FW_LOG_DEBUG 0
#define FW_LOG_INFO  1
#define FW_LOG_WARN  2
#define FW_LOG_ERROR 3

void framework_log(framework_ctx_t* ctx, int level, const char* fmt, ...);

#define FW_DEBUG(ctx, ...) framework_log(ctx, FW_LOG_DEBUG, __VA_ARGS__)
#define FW_INFO(ctx, ...)  framework_log(ctx, FW_LOG_INFO, __VA_ARGS__)
#define FW_WARN(ctx, ...)  framework_log(ctx, FW_LOG_WARN, __VA_ARGS__)
#define FW_ERROR(ctx, ...) framework_log(ctx, FW_LOG_ERROR, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* FRAMEWORK_H */
