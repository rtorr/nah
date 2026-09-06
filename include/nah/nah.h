/* Umbrella header for the NAH host API. SPDX-License-Identifier: MIT */

#ifndef NAH_H
#define NAH_H

// Core: Pure computation (no dependencies)
#include "nah_core.h"

// Semver: Semantic versioning support
#include "nah_semver.h"

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
