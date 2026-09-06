# Conan SDK example

This C++ example shows a NAK whose implementation uses Conan dependencies. The resulting package contains the SDK library, loader, resources, and deployed runtime libraries; NAH itself does not resolve those dependencies.

Use the coordinated example workflow:

```bash
cd ..
./scripts/build_all.sh
./scripts/setup_host.sh --clean
./scripts/run_apps.sh com.example.mygame
```

Set `SKIP_CONAN=1` to omit this example. Packaging uses `nah pack` and emits `com.example.gameengine-1.0.0.nak` from the Conan release build directory.
