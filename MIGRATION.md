# Migrating to NAH 3.x

NAH 3.x narrows the project to local package storage and deterministic launch contracts. It removes component URI routing and process-environment policy hooks.

## Required changes

- Replace an app manifest with root-level `nap.json` using the nested `app.identity`, `app.execution`, and optional `app.layout` sections.
- Replace a runtime manifest with root-level `nak.json` using `nak.identity`, `nak.paths`, and optional named `nak.loaders`.
- Rebuild archives with `nah pack`; do not copy old `.nap` or `.nak` files into a 2.x root.
- Use `nap.v2.json`, `app-record.v2.json`, and `nah.v2.json`. NAK schemas remain at v1.
- Replace host state with one `<root>/host/host.json` containing only `environment` and `paths`.
- Replace flat app fields and `env_vars` with the nested v2 manifest and its
  `environment` map. The parser no longer accepts compatibility aliases.
- Replace component entrypoints with separate app packages when they need independent lifecycle or launch behavior.
- Replace legacy command groups with the flat commands documented by `nah --help`.
- Reinstall NAKs before apps so app installation can pin a matching runtime record.

There is no in-place registry migration. Create a fresh root, reinstall packages, inspect each app with `nah show`, and switch the embedding host only after those contracts are correct.

Current shapes are defined by [`docs/schemas`](docs/schemas), and the compatibility boundary is defined in [`SPEC.md`](SPEC.md).
