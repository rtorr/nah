# NAH CLI Reference

## Synopsis

```
nah [OPTIONS] <COMMAND>
```

## Global Options

| Option             | Description                                                |
| ------------------ | ---------------------------------------------------------- |
| `--root <PATH>`    | NAH root directory (auto-detected from cwd or `$NAH_ROOT`) |
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
- Directory with `nak.json` at root → NAK
- Directory with `nap.json` at root → app

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

**Manifest detection:**

- Looks for `nap.json` (app) or `nak.json` (NAK) at directory root
- Package is created as a standard tar.gz archive

---

### `nah run`

Execute an installed application.

```bash
nah run com.example.app                 # Run by ID
nah run com.example.app@1.0.0           # Run specific version
nah run com.example.app -- arg1 arg2    # Pass arguments to app
```

**Options:**

- `--` - Pass remaining arguments to the application

**What it does:**

1. Loads app manifest and install record
2. Resolves NAK dependency
3. Composes launch contract (library paths, environment variables)
4. Executes the application with correct environment

---

### `nah show`

Display details about an installed package.

```bash
nah show com.example.app                # Show app details
nah show com.example.sdk                # Show NAK details
nah show com.example.app --json         # JSON output
```

**Output:**

```
com.example.app@1.0.0

Identity:
  ID: com.example.app
  Version: 1.0.0
  NAK: com.example.sdk (>=1.0.0 <2.0.0)

Execution:
  Entrypoint: bin/app
  Working directory: /nah/apps/com.example.app-1.0.0

Layout:
  Library directories: lib
  Asset directories: assets

Install:
  Location: /nah/apps/com.example.app-1.0.0
  Installed: 2026-01-21T10:30:00Z
```

---

### `nah init`

Create a new project.

```bash
nah init app ./myapp      # Create app project skeleton
nah init nak ./mysdk      # Create NAK project skeleton
nah init host ./my-nah    # Create NAH host directory
```

**Types:**

- `app` - Application project with `nap.json` template
- `nak` - NAK (SDK) project with `nak.json` template
- `host` - NAH host with `nah.json` configuration

---

## Environment Variables

| Variable      | Description                |
| ------------- | -------------------------- |
| `NAH_ROOT`    | Default NAH root directory |

## Exit Codes

| Code | Meaning                                               |
| ---- | ----------------------------------------------------- |
| 0    | Success, no warnings                                  |
| 1    | Error (critical failure or warning upgraded to error) |
| 2    | Success with warnings                                 |

## Examples

### Typical workflow

```bash
# Initialize a NAH host
nah init host ./my-nah

# Install SDK and app
nah install vendor-sdk-2.1.0.nak
nah install myapp-1.0.0.nap

# List installed packages
nah list

# Show package details
nah show com.example.myapp

# Run application
nah run com.example.myapp
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

### Working with NAKs

```bash
# Create NAK skeleton
nah init nak ./mysdk
cd mysdk

# ... build your SDK ...

# Pack for distribution
nah pack . --nak -o mysdk-1.0.0.nak

# Install for apps to use
nah install mysdk-1.0.0.nak
```
