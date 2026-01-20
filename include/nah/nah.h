/*
 * NAH - Native Application Host
 * 
 * Complete host library for composing and executing launch contracts.
 * 
 * This header includes everything:
 *   - nah_core.h     : Pure computation (types, composition, validation)
 *   - nah_json.h     : JSON parsing (requires nlohmann/json)
 *   - nah_fs.h       : Filesystem operations
 *   - nah_exec.h     : Contract execution
 *   - nah_overrides.h: NAH_OVERRIDE_* environment variable handling
 *   - nah_host.h     : High-level NahHost class
 * 
 * For pure/embeddable usage, include only nah_core.h.
 * 
 * ============================================================================
 * QUICK START
 * ============================================================================
 * 
 *   #define NAH_HOST_IMPLEMENTATION
 *   #include <nah/nah.h>
 *   
 *   auto host = nah::host::NahHost::create();
 *   int exit_code = host->executeApplication("com.example.app");
 * 
 * Or with more control:
 * 
 *   #include <nah/nah.h>
 *   
 *   // Read inputs from files
 *   auto app_json = nah::fs::read_file("nah.json");
 *   auto host_json = nah::fs::read_file("/nah/host/host.json");
 *   auto install_json = nah::fs::read_file("/nah/registry/apps/myapp.json");
 *   
 *   // Parse
 *   auto app = nah::json::parse_app_declaration(*app_json);
 *   auto host_env = nah::json::parse_host_environment(*host_json);
 *   auto install = nah::json::parse_install_record(*install_json);
 *   
 *   // Load inventory
 *   auto inventory = nah::fs::load_inventory_from_directory("/nah/registry/naks");
 *   
 *   // Compose
 *   auto result = nah::core::nah_compose(
 *       app.value, host_env.value, install.value, inventory);
 *   
 *   // Execute
 *   if (result.ok) {
 *       nah::exec::execute(result.contract);
 *   }
 * 
 * ============================================================================
 * SPDX-License-Identifier: Apache-2.0
 * ============================================================================
 */

#ifndef NAH_H
#define NAH_H

// Core: Pure computation (no dependencies)
#include "nah_core.h"

// JSON: Parsing and serialization (requires nlohmann/json)
#include "nah_json.h"

// Overrides: NAH_OVERRIDE_* parsing and application (requires nlohmann/json)
#include "nah_overrides.h"

// Filesystem: File operations (requires C++17 <filesystem>)
#include "nah_fs.h"

// Execution: Process spawning (platform-specific)
#include "nah_exec.h"

// Host: High-level API for managing a NAH root
#include "nah_host.h"

#endif // NAH_H
