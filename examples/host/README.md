# Host example

This directory contains C++ examples for inspecting contracts and embedding `NahHost`. The adjacent `host.json` is copied to the example NAH root by `scripts/setup_host.sh`.

From `examples`:

```bash
./scripts/build_all.sh
./scripts/setup_host.sh --clean
./scripts/run_apps.sh --contract
```

The setup script copies the host environment to `<root>/host/host.json` and installs packages explicitly. See [Host integration](../../docs/getting-started-host.md) for the ownership and security boundary.
