# NAH CLI Reference

## Synopsis

```
nah [OPTIONS] <COMMAND>
```

## Global Options

| Option             | Description                                                |
| ------------------ | ---------------------------------------------------------- |
| `--root <PATH>`    | NAH root directory (auto-detected from cwd or `$NAH_ROOT`) |
| `--profile <NAME>` | Use specific profile instead of active                     |
| `--json`           | Output in JSON format                                      |
| `--trace`          | Include provenance information                             |
| `-v, --verbose`    | Show detailed progress                                     |
| `-q, --quiet`      | Suppress non-essential output                              |
| `-V, --version`    | Show version                                               |
| `-h, --help`       | Show help                                                  |

## Root Auto-Detection

NAH automatically finds the root directory:

1. `--root` flag (explicit)
2. `NAH_ROOT` environment variable
3. Walk up from cwd looking for valid NAH root (`.nah/` marker or `host/` + `apps/` directories)
4. Default: `~/.nah` (auto-created on first install)

When using the default root, install messages show `(default)` suffix:
```
Installed: com.example.app@1.0.0 → ~/.nah (default)
```

## Commands

### `nah install`

Install an app or NAK package. Type is auto-detected from file extension.

```bash
nah install myapp.nap              # Install app
nah install mysdk.nak              # Install NAK
nah install ./myapp/               # Pack and install directory
nah install https://example.com/app.nap  # Install from URL
```

**Options:**

- `-f, --force` - Overwrite existing installation
- `--app` - Force install as app (skip auto-detection)
- `--nak` - Force install as NAK (skip auto-detection)

**Detection logic:**

- `.nap` extension → app
- `.nak` extension → NAK
- Directory with `META/nak.json` → NAK
- Directory with `manifest.json` or embedded manifest → app

---

### `nah uninstall`

Remove an installed app or NAK.

```bash
nah uninstall com.example.app
nah uninstall com.example.sdk@1.0.0
```

**Options:**

- `--app` - Force uninstall as app
- `--nak` - Force uninstall as NAK

If both an app and NAK exist with the same ID, you must specify `--app` or `--nak`.

---

### `nah list`

List installed apps and NAKs.

```bash
nah list               # Show all
nah list --apps        # Apps only
nah list --naks        # NAKs only
nah list --json        # Machine-readable output
```

**Output format:**

```
Apps:
  com.example.app@1.0.0      → com.example.sdk@2.1.0
  com.other.tool@2.0.0       → com.example.sdk@2.1.0

NAKs:
  com.example.sdk@2.1.0      (used by 2 apps)
  com.example.sdk@1.5.0      (unused)
```

---

### `nah pack`

Create a `.nap` or `.nak` package from a directory.

```bash
nah pack ./myapp/                      # Auto-detect type
nah pack ./myapp/ -o myapp-1.0.0.nap   # Specify output
nah pack ./mysdk/ --nak                # Force NAK type
```

**Options:**

- `-o, --output <FILE>` - Output file path (auto-generated if omitted)
- `--app` - Force pack as app
- `--nak` - Force pack as NAK

**Manifest handling:**

- If `manifest.nah` exists, uses it directly
- If `manifest.json` exists, automatically converts to binary format
- If neither exists, looks for embedded manifest in binaries

---

### `nah status`

Show status, validate files, or diagnose issues. This unified command replaces the old `contract show`, `doctor`, `validate`, and `format` commands.

```bash
# Overview of NAH root
nah status

# App contract details
nah status com.example.app

# With provenance information
nah status com.example.app --trace

# Validate a configuration file
nah status profile.json

# Validate and format a file
nah status profile.json --fix

# Compare contract with another profile
nah status com.example.app --diff staging
```

**Options:**

- `--fix` - Attempt to fix issues (also formats JSON files)
- `--diff <PROFILE>` - Compare contract with another profile
- `--overrides <FILE>` - Apply overrides file to contract

**Output for apps:**

```
Application: com.example.app v1.0.0
NAK: com.example.sdk v2.1.0
Binary: /nah/apps/com.example.app-1.0.0/bin/myapp
CWD: /nah/apps/com.example.app-1.0.0

Library Paths (DYLD_LIBRARY_PATH):
  /nah/naks/com.example.sdk/2.1.0/lib

Environment (NAH_*):
  NAH_APP_ID=com.example.app
  NAH_APP_VERSION=1.0.0
  NAH_NAK_ROOT=/nah/naks/com.example.sdk/2.1.0

Run with --trace to see where each value comes from.
```

---

### `nah init`

Create a new project.

```bash
nah init app ./myapp      # Create app project skeleton
nah init nak ./mysdk      # Create NAK project skeleton
nah init root ./my-nah    # Create NAH root directory
```

**Types:**

- `app` - Application project with manifest template
- `nak` - NAK (SDK) project with META/nak.json
- `root` - NAH root with host/profiles and registry directories

---

### `nah profile`

Manage host profiles.

```bash
nah profile list           # List available profiles
nah profile set production # Set active profile
```

---

### `nah manifest generate`

Generate a binary manifest from JSON input. Used for bundle applications (JavaScript, Python) that can't embed manifests in native binaries.

```bash
nah manifest generate manifest.json -o manifest.nah
```

**Options:**

- `-o, --output <FILE>` - Output file path (required)

**Input format:**

```json
{
  "app": {
    "id": "com.example.myapp",
    "version": "1.0.0",
    "nak_id": "com.example.runtime",
    "nak_version_req": ">=2.0.0",
    "entrypoint": "bundle.js"
  }
}
```

---

## Environment Variables

| Variable      | Description                |
| ------------- | -------------------------- |
| `NAH_ROOT`    | Default NAH root directory |
| `NAH_PROFILE` | Override active profile    |

## Exit Codes

| Code | Meaning                                               |
| ---- | ----------------------------------------------------- |
| 0    | Success, no warnings                                  |
| 1    | Error (critical failure or warning upgraded to error) |
| 2    | Success with warnings                                 |

## Examples

### Typical workflow

```bash
# Initialize a NAH root
nah init root ./my-nah
cd my-nah

# Install SDK and app
nah install ../vendor-sdk-2.1.0.nak
nah install ../myapp-1.0.0.nap

# Check status
nah list
nah status com.example.myapp

# Diagnose issues
nah status com.example.myapp --trace
```

### Creating and distributing an app

```bash
# Create app skeleton
nah init app ./myapp
cd myapp

# ... develop your app ...

# Pack for distribution
nah pack . -o myapp-1.0.0.nap

# Install elsewhere
nah install myapp-1.0.0.nap
```

### Validating configuration

```bash
# Check if a profile is valid
nah status profile.json

# Format and validate
nah status profile.json --fix

# Check an install record
nah status registry/installs/com.example.app-1.0.0-uuid.json
```
