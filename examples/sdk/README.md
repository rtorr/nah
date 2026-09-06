# Framework SDK example

This C example builds `com.example.sdk@1.2.3`: a shared library, resources, and a default loader packaged as a NAK.

```bash
cmake -S . -B build -DNAH_CLI=/path/to/nah
cmake --build build --target nah_package
```

The output is `build/com.example.sdk-1.2.3.nak`. Packaging goes through `nah pack`, so the example uses the same validation and deterministic archive format as normal packages.

The paired app is in [`../apps/app`](../apps/app). From the parent `examples` directory, `scripts/build_all.sh` builds the coordinated set.
