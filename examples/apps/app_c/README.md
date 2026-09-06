# C++ app example

This is the C++ counterpart to `apps/app`. It links the framework SDK while its `nap.json` requests `com.example.sdk` at runtime.

From the parent `examples` directory:

```bash
./scripts/build_all.sh
./scripts/setup_host.sh --clean
./scripts/run_apps.sh com.example.app_c
```

The package is built through `nah pack` as `com.example.app_c-1.0.0.nap`.
