# NAH JSON Schemas

This directory contains JSON Schema definitions for NAH manifest and record formats.

## Schema Files

### Application Manifests

- **`nap.v1.json`** - App Manifest (`nap.json`)
  - Developer-owned, immutable manifest packaged in `.nap` files
  - Defines app identity, NAK requirements, execution details, and permissions
  - URL: `https://nah.rtorr.com/schemas/nap.v1.json`

### NAK Manifests

- **`nak.v1.json`** - NAK Manifest (`nak.json`)
  - Vendor-owned, immutable manifest packaged in `.nak` files
  - Defines NAK identity, paths, loader configuration, and environment
  - URL: `https://nah.rtorr.com/schemas/nak.v1.json`

### Host Configuration

- **`nah.v1.json`** - Host Configuration (`nah.json`)
  - Admin-owned, mutable host configuration
  - Defines host environment, paths, and override policies
  - URL: `https://nah.rtorr.com/schemas/nah.v1.json`

### Install Records

- **`app-record.v1.json`** - App Install Record
  - Host-owned record of an installed app instance
  - Created by `nah app install`, stored in `<nah_root>/registry/apps/`
  - URL: `https://nah.rtorr.com/schemas/app-record.v1.json`

- **`nak-record.v1.json`** - NAK Install Record
  - Host-owned record of an installed NAK instance
  - Created by `nah nak install`, stored in `<nah_root>/registry/naks/`
  - URL: `https://nah.rtorr.com/schemas/nak-record.v1.json`

## Usage

### In Manifest Files

Reference schemas in your manifest files for editor autocomplete and validation:

```json
{
  "$schema": "https://nah.rtorr.com/schemas/nap.v1.json",
  "app": {
    "identity": {
      "id": "com.example.myapp",
      "version": "1.0.0"
    },
    "execution": {
      "entrypoint": "bin/myapp"
    }
  }
}
```

### Environment Variable Operations

All environment fields support both simple string values and operation objects:

```json
{
  "environment": {
    "SIMPLE_VAR": "value",
    "PATH": {
      "op": "prepend",
      "value": "/custom/bin",
      "separator": ":"
    },
    "REMOVE_ME": {
      "op": "unset"
    }
  }
}
```

**Supported operations:**
- `set` - Set the value (default for string values)
- `prepend` - Prepend value with separator
- `append` - Append value with separator
- `unset` - Remove the variable

## Schema Deployment

These schemas are deployed to `https://nah.rtorr.com/schemas/` via GitHub Pages.

During a release:

1. Schemas are copied from `/Users/rtorruellas/personal/nah/docs/schemas/` to the GitHub Pages deployment
2. They become accessible at their canonical URLs
3. Manifests can reference them for validation and autocomplete

## Validation

Validate schemas locally:

```bash
# Validate all schemas are valid JSON
cd docs/schemas
for f in *.json; do 
  python3 -m json.tool "$f" > /dev/null && echo "✓ $f" || echo "✗ $f"
done
```

## Version Policy

Schema identifiers ending in `.v1` are additive-only:
- New optional fields may be added
- New enum values may be added to non-critical fields
- Breaking changes require a new schema version (e.g., `.v2`)

Implementations **must** ignore unknown keys to support forward compatibility.

## Specification

For complete normative specification of these formats, see:
- [`SPEC.md`](../../SPEC.md) - Complete NAH specification
- [`UNIFIED_JSON_ANALYSIS.md`](../../UNIFIED_JSON_ANALYSIS.md) - Design analysis and rationale

