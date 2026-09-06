# Troubleshooting

Global options precede the command:

```bash
nah --root ./nah-root --json show com.example.app
```

## Package is rejected

- Confirm an app contains exactly one root `nap.json`, or a NAK contains exactly one root `nak.json`.
- Validate JSON against the schema in [`docs/schemas`](schemas).
- Keep every declared path relative to the package root.
- Remove symlinks and special files; packages accept regular files and directories only.
- Use `.nap` for an app archive and `.nak` for a NAK archive.

## App cannot compose or run

```bash
nah --root ./nah-root list
nah --root ./nah-root show com.example.app
nah --root ./nah-root --trace show com.example.app
```

If the app requires a NAK, install a matching NAK and reinstall the app so its record is pinned. A missing placeholder, escaped path, absent entrypoint, or unavailable pinned record is a composition error.

## Host configuration is ignored

The runtime reads `<root>/host/host.json`. Its fields are `environment`, `paths`, and `overrides`; there is no outer `host` object in newly generated files. Run `nah init --host <root>` for a usable skeleton.

## Root selection

`--root` wins over `NAH_ROOT`; otherwise the CLI uses `~/.nah`. Installation creates the managed directory structure. Use an explicit root in automation.

For exact command syntax, run `nah <command> --help` or see the [CLI reference](cli.md).
