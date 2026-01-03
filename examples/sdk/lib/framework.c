#include "framework/framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

struct framework_ctx {
    char* app_id;
    char* app_version;
    char* app_root;
    char* sdk_root;
    int log_level;
};

static char* safe_strdup(const char* s) {
    return s ? strdup(s) : NULL;
}

bool framework_is_nah_managed(void) {
    return getenv("NAH_APP_ID") != NULL;
}

framework_ctx_t* framework_init(const fw_init_options_t* opts) {
    framework_ctx_t* ctx = calloc(1, sizeof(framework_ctx_t));
    if (!ctx) return NULL;
    
    ctx->log_level = opts ? opts->log_level : FW_LOG_INFO;
    
    if (opts && opts->use_nah_env) {
        ctx->app_id = safe_strdup(getenv("NAH_APP_ID"));
        ctx->app_version = safe_strdup(getenv("NAH_APP_VERSION"));
        ctx->app_root = safe_strdup(getenv("NAH_APP_ROOT"));
        ctx->sdk_root = safe_strdup(getenv("NAH_NAK_ROOT"));
    } else {
        ctx->app_id = safe_strdup(opts ? opts->app_id : "unknown");
        ctx->app_root = safe_strdup(opts ? opts->app_root : ".");
    }
    
    if (!ctx->app_id) ctx->app_id = safe_strdup("unknown");
    if (!ctx->app_version) ctx->app_version = safe_strdup("0.0.0");
    if (!ctx->app_root) ctx->app_root = safe_strdup(".");
    if (!ctx->sdk_root) ctx->sdk_root = safe_strdup(".");
    
    return ctx;
}

void framework_shutdown(framework_ctx_t* ctx) {
    if (!ctx) return;
    free(ctx->app_id);
    free(ctx->app_version);
    free(ctx->app_root);
    free(ctx->sdk_root);
    free(ctx);
}

const char* framework_get_app_id(framework_ctx_t* ctx) {
    return ctx ? ctx->app_id : NULL;
}

const char* framework_get_app_version(framework_ctx_t* ctx) {
    return ctx ? ctx->app_version : NULL;
}

const char* framework_get_app_root(framework_ctx_t* ctx) {
    return ctx ? ctx->app_root : NULL;
}

const char* framework_get_sdk_root(framework_ctx_t* ctx) {
    return ctx ? ctx->sdk_root : NULL;
}

void framework_log(framework_ctx_t* ctx, int level, const char* fmt, ...) {
    if (!ctx || level < ctx->log_level) return;
    
    const char* level_str[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    printf("[%s] [%s] ", level_str[level], ctx->app_id);
    
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    
    printf("\n");
}

int framework_run(framework_ctx_t* ctx, const fw_lifecycle_callbacks_t* callbacks, void* user_data) {
    if (!ctx || !callbacks) return 1;
    
    int result = 0;
    
    if (callbacks->on_start) {
        result = callbacks->on_start(ctx, user_data);
    }
    
    if (callbacks->on_stop) {
        callbacks->on_stop(ctx, user_data);
    }
    
    return result;
}
