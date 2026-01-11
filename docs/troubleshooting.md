# Troubleshooting

Common issues and solutions for NAH.

## Quick Diagnostics

```bash
# Overview of your NAH root
nah status

# Check a specific app
nah status com.example.app

# See where each value comes from
nah status com.example.app --trace
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
1. Create a new root: `nah init root /path/to/nah`
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
   - Apps: `manifest.json` or binary with embedded manifest
   - NAKs: `META/nak.json`
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
3. Check which NAK is needed: `nah status <app-id> --trace`

### "Invalid manifest"

**Symptom:**
```
error: manifest parse failed: missing required field 'id'
```

**Cause:** The manifest in your package is malformed.

**Solutions:**
1. Check manifest structure matches the schema
2. For embedded manifests, ensure correct binary section
3. Validate the file: `nah status manifest.json`

## Debugging Techniques

### Trace Mode

Add `--trace` to see provenance of every value:

```bash
nah status com.example.app --trace
```

This shows:
- Where each environment variable came from (manifest, NAK, profile)
- Which NAK version was selected and why
- Profile settings that affected the contract

### Compare Profiles

See how different profiles affect an app:

```bash
nah status com.example.app --diff staging
```

This shows all differences between the current profile and `staging`.

### Validate Files

Check if a configuration file is valid:

```bash
nah status profile.json
nah status install-record.json
```

### JSON Output

For scripting or detailed inspection:

```bash
nah status com.example.app --json | jq '.environment'
```

## Environment Variables

| Variable | Purpose |
|----------|---------|
| `NAH_ROOT` | Default NAH root directory |
| `NAH_PROFILE` | Override active profile |

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
