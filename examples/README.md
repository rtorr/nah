# Examples

The examples exercise three roles:

- `sdk` and `conan-sdk`: NAK packages;
- `apps`: native and script app packages;
- `host`: embedding and contract-inspection examples.

Build the packages, install them into an isolated root, then run or inspect apps:

```bash
./scripts/build_all.sh
./scripts/setup_host.sh --clean
./scripts/run_apps.sh --contract
./scripts/run_apps.sh com.example.app
```

Set `NAH_CLI` to choose a CLI binary and `NAH_ROOT` to choose the managed root. The scripts otherwise use the repository build and `examples/demo_nah_root`.

For a new package, start with the CLI rather than copying an example’s generated files:

```bash
nah init --app --id com.example.new-app ./new-app
nah init --nak --id com.example.new-sdk ./new-sdk
```

See the [app](../docs/getting-started-app.md), [NAK](../docs/getting-started-nak.md), and [host](../docs/getting-started-host.md) guides for current manifest shapes.
