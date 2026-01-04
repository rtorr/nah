#ifndef NAH_EXPORT_HPP
#define NAH_EXPORT_HPP

/**
 * @file export.hpp
 * @brief Cross-platform shared library export/import macros.
 *
 * When building NAH as a shared library:
 * - Define NAH_SHARED when using the library
 * - NAH_BUILDING_SHARED is defined automatically during library compilation
 *
 * Usage in headers:
 *   NAH_API void my_function();
 *   class NAH_API MyClass { ... };
 */

#if defined(_WIN32) || defined(_WIN64)
    // Windows
    #ifdef NAH_BUILDING_SHARED
        #define NAH_API __declspec(dllexport)
    #elif defined(NAH_SHARED)
        #define NAH_API __declspec(dllimport)
    #else
        #define NAH_API
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    // GCC/Clang - use visibility attribute for shared libraries
    #ifdef NAH_BUILDING_SHARED
        #define NAH_API __attribute__((visibility("default")))
    #else
        #define NAH_API
    #endif
#else
    // Other compilers - no decoration
    #define NAH_API
#endif

#endif // NAH_EXPORT_HPP
