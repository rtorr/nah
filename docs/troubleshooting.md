# Troubleshooting

Common issues and solutions for NAH.

## Quick Diagnostics

```bash
# List installed packages
nah list

# Show details for a specific app
nah show com.example.app

# Run with verbose output
nah run com.example.app --verbose
```

## Common Errors

### "NAH root directory does not exist"

**Symptom:**
```
error: NAH root directory does not exist: /nah
hint: Initialize a new NAH root with: nah init root /nah
```

**Cause:** NAH looks for a root directory but none exists or was specified.

**Solutions:**
1. Create a new host: `nah init host /path/to/nah`
2. Specify an existing root: `nah --root /existing/nah list`
3. Set the environment variable: `export NAH_ROOT=/path/to/nah`
4. Run from within a directory containing `host/` or `.nah/`

### "Cannot detect package type"

**Symptom:**
```
error: Cannot detect package type for: ./mydir/
hint: The source doesn't have a recognized extension (.nap or .nak)
      and couldn't be detected from contents.
```

**Cause:** NAH couldn't determine if this is an app or NAK.

**Solutions:**
1. Use proper extension: rename to `.nap` for apps, `.nak` for NAKs
2. For directories, ensure you have:
   - Apps: `nap.json` at directory root
   - NAKs: `nak.json` at directory root
3. Force the type: `nah install ./mydir --app` or `--nak`

### "Not found: com.example.app"

**Symptom:**
```
error: Not found: com.example.app
hint: Run 'nah list' to see installed packages.

Similar installed packages:
  com.example.app-v2@1.0.0 (app)
```

**Cause:** The specified app or NAK isn't installed, or you mistyped the ID.

**Solutions:**
1. Check installed packages: `nah list`
2. Verify the exact ID (it's case-sensitive)
3. Install the package: `nah install <package-file>`

### "Ambiguous ID"

**Symptom:**
```
error: Ambiguous ID: com.example.thing
hint: Both an app and a NAK exist with this ID.
      Use --app or --nak to specify which to uninstall
```

**Cause:** You have both an app and NAK with the same ID.

**Solutions:**
1. Specify which one: `nah uninstall com.example.thing --app`
2. Use version to disambiguate: `nah uninstall com.example.thing@1.0.0`

### "NAK not available"

**Symptom:**
```
warning: pinned NAK is not available
```

**Cause:** An app was installed with a NAK that has since been removed.

**Solutions:**
1. Reinstall the NAK: `nah install <nak-file>`
2. Reinstall the app to pin to a different NAK version
3. Check which NAK is needed: `nah show <app-id>`

### "Invalid manifest"

**Symptom:**
```
error: manifest parse failed: missing required field 'id'
```

**Cause:** The manifest in your package is malformed.

**Solutions:**
1. Check manifest structure matches the schema: https://nah.rtorr.com/schemas/
2. Validate JSON syntax: `cat nap.json | jq .`
3. Use `$schema` in your manifest for IDE validation

## Debugging Techniques

### JSON Output

For detailed inspection and scripting:

```bash
nah show com.example.app --json
nah list --json
```

### Verbose Mode

See detailed progress during operations:

```bash
nah install myapp.nap --verbose
nah run com.example.app --verbose
```

### Check Manifests

Inspect manifests in installed packages:

```bash
# App manifest
cat ~/.nah/apps/com.example.app-1.0.0/nap.json

# NAK manifest
cat ~/.nah/naks/com.example.sdk/1.0.0/nak.json

# Host configuration
cat ~/.nah/host/nah.json
```

### Validate JSON

Check JSON syntax:

```bash
cat nap.json | jq .
cat nak.json | jq .
```

## Environment Variables

| Variable | Purpose |
|----------|---------|
| `NAH_ROOT` | Default NAH host directory |

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success, no warnings |
| 1 | Error (critical failure or warning upgraded to error) |
| 2 | Success with warnings |

## Getting Help

1. Check the [CLI Reference](cli.md) for command details
2. Read [Concepts](concepts.md) for terminology
3. See [examples/](../examples/) for working code
4. File issues at https://github.com/rtorr/nah/issues
