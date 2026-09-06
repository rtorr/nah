# Contributing

NAH requires CMake 3.21+, a C++17 compiler, and network access during the first configuration to obtain pinned dependencies.

```bash
cmake -S . -B build -DNAH_ENABLE_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Keep changes within the project boundary: local package handling, install records, launch-contract composition, inspection, and execution. Remote acquisition, signature services, sandbox implementations, and process supervision belong in consuming systems.

Before opening a pull request, run the test suite and `scripts/format.sh --check`. New behavior should include a focused test and corresponding schema or specification change when it alters a public contract.

Releases are maintainer operations:

```bash
./scripts/release.sh 2.0.17
```

The release script verifies a clean tree, builds and tests in a temporary directory, updates `VERSION`, creates an annotated tag, and pushes the commit and tag. Contributions are licensed under the [MIT License](LICENSE).
