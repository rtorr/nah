# Contributing to NAH

## Development Setup

```bash
# Clone the repository
git clone https://github.com/rtorr/nah.git
cd nah

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release -DNAH_ENABLE_TESTS=ON
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure
```

### Dependencies

- CMake 3.16+
- C++17 compiler
- OpenSSL
- libcurl
- zlib

On macOS:
```bash
brew install cmake openssl curl
```

On Ubuntu/Debian:
```bash
sudo apt-get install cmake libssl-dev libcurl4-openssl-dev zlib1g-dev
```

## Running Examples

```bash
cd examples

# Build all examples
NAH_CLI=/path/to/build/tools/nah/nah ./scripts/build_all.sh

# Setup host with NAKs and apps
NAH_CLI=/path/to/build/tools/nah/nah ./scripts/setup_host.sh

# Run the apps
NAH_CLI=/path/to/build/tools/nah/nah ./scripts/run_apps.sh
```

## Releasing

Releases are managed via the release script. The script:

1. Updates the VERSION file
2. Updates version in CMakeLists.txt, conanfile.py, and npm/package.json
3. Creates a git commit and tag
4. Pushes to origin, triggering GitHub Actions

### Creating a Release

```bash
# Interactive (will prompt for confirmation)
./scripts/release.sh 1.2.0

# Non-interactive (for CI/automation)
./scripts/release.sh --yes 1.2.0
```

### Version Format

Versions follow [Semantic Versioning 2.0.0](https://semver.org/):

- `MAJOR.MINOR.PATCH` (e.g., `1.2.3`)
- Pre-release: `1.2.3-beta.1`
- Build metadata: `1.2.3+build.456`

### What Happens After Release

When a tag is pushed, GitHub Actions automatically:

1. Builds binaries for Linux (x64, arm64) and macOS (x64, arm64)
2. Creates a GitHub Release with the binaries
3. Publishes to npm as `@rtorr/nah`

Monitor progress at: https://github.com/rtorr/nah/actions

## Code Style

- Use 4 spaces for indentation (no tabs)
- No trailing whitespace
- C++ files use `.cpp` and `.hpp` extensions
- Follow existing patterns in the codebase

## Pull Requests

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Run tests: `ctest --test-dir build`
5. Submit a pull request

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
