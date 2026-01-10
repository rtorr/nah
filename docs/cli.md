# NAH CLI Reference

## Synopsis

```
nah [OPTIONS] <COMMAND>
```

## Global Options

| Option | Description |
|--------|-------------|
| `--root <PATH>` | NAH root directory (default: `/nah` or `$NAH_ROOT`) |
| `--profile <NAME>` | Use specific profile instead of active |
| `--json` | Output in JSON format |
| `--help` | Show help |
| `--version` | Show version |

## Commands

### Application Commands

#### `nah app list`

List all installed applications.

```bash
nah --root ./my-nah app list
```

#### `nah app show <APP_ID>`

Show details of an installed application.

```bash
nah --root ./my-nah app show com.example.myapp
```

#### `nah app install <SOURCE>`

Install an application from a `.nap` package. Source can be a local file path or an HTTPS URL.

```bash
# From local file
nah --root ./my-nah app install myapp-1.0.0.nap

# From URL
nah --root ./my-nah app install https://example.com/myapp-1.0.0.nap
```

#### `nah app uninstall <APP_ID>`

Remove an installed application.

```bash
nah --root ./my-nah app uninstall com.example.myapp
```

#### `nah app verify <APP_ID>`

Verify an installed application is healthy.

```bash
nah --root ./my-nah app verify com.example.myapp
```

#### `nah app init <NAME>`

Create a new application project skeleton.

```bash
nah app init myapp
```

#### `nah app pack <DIR> -o <OUTPUT>`

Create a `.nap` package from a directory.

```bash
nah app pack ./myapp -o myapp-1.0.0.nap
```

---

### NAK Commands

#### `nah nak list`

List all installed NAKs.

```bash
nah --root ./my-nah nak list
```

#### `nah nak show <NAK_ID>`

Show NAK details.

```bash
nah --root ./my-nah nak show com.example.sdk
```

Options:
- `--version <VERSION>` - Show specific version

#### `nah nak install <SOURCE>`

Install a NAK from a `.nak` pack. Source can be a local file path or an HTTPS URL.

```bash
# From local file
nah --root ./my-nah nak install mysdk-1.0.0.nak

# From URL
nah --root ./my-nah nak install https://example.com/mysdk-1.0.0.nak
```

#### `nah nak path <NAK_ID@VERSION>`

Print the installation path of a NAK.

```bash
nah --root ./my-nah nak path com.example.sdk@1.0.0
```

#### `nah nak init <NAME>`

Create a new NAK project skeleton.

```bash
nah nak init mysdk
```

#### `nah nak pack <DIR> -o <OUTPUT>`

Create a `.nak` pack from a directory.

```bash
nah nak pack ./mysdk -o mysdk-1.0.0.nak
```

---

### Profile Commands

#### `nah profile init <PATH>`

Initialize a new NAH root directory.

```bash
nah profile init ./my-nah-root
```

Options:
- `--profile <NAME>` - Initial profile name (default: `default`)

#### `nah profile list`

List all available profiles.

```bash
nah --root ./my-nah profile list
```

#### `nah profile show [NAME]`

Display a profile's configuration.

```bash
nah --root ./my-nah profile show
nah --root ./my-nah profile show staging
```

#### `nah profile set <NAME>`

Set the active profile.

```bash
nah --root ./my-nah profile set production
```

#### `nah profile validate <FILE>`

Validate a profile file.

```bash
nah profile validate host-profile.json
```

---

### Contract Commands

#### `nah contract show <APP_ID>`

Show the launch contract for an application.

```bash
nah --root ./my-nah contract show com.example.myapp
nah --root ./my-nah --json contract show com.example.myapp
```

#### `nah contract explain <APP_ID> <VALUE>`

Explain where a contract value comes from.

```bash
nah --root ./my-nah contract explain com.example.myapp LD_LIBRARY_PATH
```

#### `nah contract diff <APP_ID> <PROFILE1> <PROFILE2>`

Compare contracts between two profiles.

```bash
nah --root ./my-nah contract diff com.example.myapp default production
```

#### `nah contract resolve <APP_ID>`

Explain NAK selection for an application.

```bash
nah --root ./my-nah contract resolve com.example.myapp
```

---

### Manifest Commands

#### `nah manifest show <PATH>`

Display manifest from a binary or installed app.

```bash
nah manifest show ./myapp
nah --root ./my-nah manifest show com.example.myapp
```

#### `nah manifest generate <JSON_FILE> -o <OUTPUT>`

Generate a binary TLV manifest from a JSON input file. This is used for bundle applications (JavaScript, Python, etc.) that cannot embed a manifest in a native binary.

```bash
nah manifest generate manifest.json -o manifest.nah
```

Options:
- `-o, --output` - Output file path (required)
- `--json` - Output result as JSON

**Input Format**

The input file must use the `nah.manifest.input.v2` schema:

```json
{
  "$schema": "nah.manifest.input.v2",
  "app": {
    "id": "com.example.myapp",
    "version": "1.0.0",
    "nak_id": "com.example.runtime",
    "nak_version_req": ">=2.0.0",
    "entrypoint": "bundle.js",
    "description": "My application",
    "author": "Developer Name",
    "license": "MIT",
    "homepage": "https://example.com",
    "entrypoint_args": ["--mode", "production"],
    "lib_dirs": ["lib"],
    "asset_dirs": ["assets"],
    "exports": [
      {
        "id": "config",
        "path": "share/config.json",
        "type": "application/json"
      }
    ],
    "environment": {
      "NODE_ENV": "production"
    },
    "permissions": {
      "filesystem": ["read:app://assets/*"],
      "network": ["connect:https://api.example.com:443"]
    }
  }
}
```

**Required Fields**

| Field | Description |
|-------|-------------|
| `$schema` | Must be `nah.manifest.input.v2` |
| `app.id` | Unique identifier (reverse domain notation) |
| `app.version` | SemVer version string |
| `app.nak_id` | NAK this app depends on |
| `app.nak_version_req` | Version requirement (SemVer range) |
| `app.entrypoint` | Relative path to entry file |

**Path Validation**

- All paths (`entrypoint`, `lib_dirs`, `asset_dirs`, `exports.path`) must be relative
- Absolute paths cause validation failure
- Paths containing `..` cause validation failure

**Permission Format**

- Filesystem: `<operation>:<selector>` where operation is `read`, `write`, or `execute`
- Network: `<operation>:<selector>` where operation is `connect`, `listen`, or `bind`

See [Getting Started: Bundle Apps](getting-started-bundle.md) for a complete workflow.

---

### Diagnostic Commands

#### `nah doctor <APP_ID>`

Diagnose and report issues with an application.

```bash
nah --root ./my-nah doctor com.example.myapp
```

Options:
- `--fix` - Attempt to fix issues automatically

#### `nah validate <TYPE> <FILE>`

Validate configuration files.

Types: `profile`, `manifest`, `nak`

```bash
nah validate profile host-profile.json
nah validate manifest manifest.json
nah validate nak META/nak.json
```

---

### Utility Commands

#### `nah format <FILE>`

Format JSON configuration files.

```bash
nah format host-profile.json
```

Options:
- `--check` - Check formatting without modifying
- `--diff` - Show diff of changes

---

## Environment Variables

| Variable | Description |
|----------|-------------|
| `NAH_ROOT` | Default NAH root directory |
| `NAH_PROFILE` | Default profile name |
| `NAH_JSON` | Set to `1` for JSON output by default |

## Exit Codes

| Code | Description |
|------|-------------|
| 0 | Success |
| 1 | General error |
| 2 | Invalid arguments |
| 3 | File not found |
| 4 | Validation error |
| 5 | Contract composition error |
