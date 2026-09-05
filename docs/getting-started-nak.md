# NAK packages

A NAK is a local runtime or SDK that contributes loaders, libraries, resources, and environment operations.

```bash
nah init --nak --id com.example.runtime ./runtime
```

A loader is named so an app or installer can select it:

```json
{
  "$schema": "https://nah.rtorr.com/schemas/nak.v1.json",
  "nak": {
    "identity": {"id": "com.example.runtime", "version": "1.0.0"},
    "paths": {"lib_dirs": ["lib"]},
    "loaders": {
      "default": {
        "exec_path": "bin/runtime",
        "args_template": ["{NAH_APP_ENTRY}"]
      }
    }
  }
}
```

All declared paths must stay inside the package. Loader executables and library directories must exist when installed.

```bash
nah pack ./runtime --output runtime.nak
nah --root ./test-root install runtime.nak
nah --root ./test-root which com.example.runtime
```

Apps are pinned at install time to the highest installed NAK version satisfying their semantic-version requirement. See the [NAK schema](schemas/nak.v1.json).
