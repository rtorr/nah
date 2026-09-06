# Component suite example

This app exposes multiple component entrypoints under one installed package. It demonstrates component discovery and URI-based launch without introducing another package format.

```bash
cmake -S . -B build -DNAH_CLI=/path/to/nah
cmake --build build --target nah_package
nah --root ./nah-root install build/com.example.suite-1.0.0.nap
nah --root ./nah-root components
nah --root ./nah-root launch com.example.suite://editor/open
```

The component declarations live in the generated `nap.json`; each entrypoint is validated as part of normal package installation.
