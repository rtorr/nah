# Migrating to NAH 2.x

NAH 2.x is the current JSON and tar-based format. Binary manifests and old package archives are not accepted; rebuild them from source.

## Required changes

- Replace an app manifest with root-level `nap.json` using the nested `app.identity`, `app.execution`, and optional `app.layout` sections.
- Replace a runtime manifest with root-level `nak.json` using `nak.identity`, `nak.paths`, and optional named `nak.loaders`.
- Rebuild archives with `nah pack`; do not copy old `.nap` or `.nak` files into a 2.x root.
- Replace profile-based host state with one `<root>/host/host.json` containing `environment`, `paths`, and `overrides`.
- Replace legacy command groups with the flat commands documented by `nah --help`.
- Reinstall NAKs before apps so app installation can pin a matching runtime record.

There is no in-place registry migration. Create a fresh root, install rebuilt packages, inspect each app with `nah show`, and switch the embedding host only after those contracts are correct.

Current shapes are defined by [`docs/schemas`](docs/schemas), and the compatibility boundary is defined in [`SPEC.md`](SPEC.md).
