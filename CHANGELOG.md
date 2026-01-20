# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.0.0] - 2026-01-20

### Changed
- **BREAKING**: Complete architectural redesign to header-only library
- **BREAKING**: Moved from traditional C++ library to pure functional composition engine
- **BREAKING**: Reorganized namespaces (nah:: → nah::core::, nah::json::, nah::fs::, nah::exec::, nah::overrides::)
- **BREAKING**: Changed composition API to use CompositionOptions struct
- **BREAKING**: Renamed error_message to critical_error_context in result types
- CLI completely rewritten with modular command structure
- No longer uses system libraries, all dependencies via FetchContent or Conan
- Reduced test suite from 219 to 57 comprehensive tests
- Improved binary manifest (TLV) parsing with proper endianness handling

### Added
- New header files: nah_core.h, nah_json.h, nah_fs.h, nah_exec.h, nah_overrides.h
- Native tar/gzip support via FetchContent zlib
- Full support for embedded manifests
- Improved error handling with Result<T> pattern throughout
- Migration guide for v1.0 to v2.0 transition

### Removed
- All separate compilation units (src/ directory)
- Old header files (capabilities.hpp, compose.hpp, contract.hpp, etc.)
- System library dependencies
- Migration path from v1.0 (breaking change)

### Fixed
- Path handling in list and run commands
- Binary manifest parsing endianness issues
- Exit code propagation in CLI commands

## [1.0.24] - 2025-01-14

### Added
- Added nah host install command for declarative host setup
- Made install commands idempotent
- Added nah nak and nah app subcommand groups

### Fixed
- Prevented duplicate app registry entries on reinstall

## Previous Versions

See git history for changes before v1.0.24.