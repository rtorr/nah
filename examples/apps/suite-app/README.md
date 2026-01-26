# Multi-Component Suite Example

This example demonstrates NAH's component architecture with a document processing suite.

## Components

The suite provides three components:

1. **Editor** (`com.example.suite://editor`)
   - Standalone: Yes
   - Hidden: No
   - Edit text documents
   - Demonstrates query parameter parsing

2. **Viewer** (`com.example.suite://viewer`)
   - Standalone: Yes
   - Hidden: No
   - View documents in read-only mode
   - Demonstrates referrer tracking

3. **Converter** (`com.example.suite://converter`)
   - Standalone: No (utility component)
   - Hidden: Yes
   - Convert between document formats
   - Demonstrates hidden components

## Building

```bash
cd examples/apps/suite-app
cmake -B build
cmake --build build
cmake --build build --target nah_package
```

This creates `build/package/com.example.suite-1.0.0.nap`

## Installation

```bash
# Using project root NAH CLI
../../../build/tools/nah/nah install build/package/com.example.suite-1.0.0.nap

# Or if NAH is installed globally
nah install build/package/com.example.suite-1.0.0.nap
```

## Usage

### List components
```bash
nah components
nah components --app com.example.suite
```

### Launch the main launcher
```bash
nah run com.example.suite
```

### Launch individual components
```bash
# Launch editor
nah launch com.example.suite://editor

# Launch editor with file parameter
nah launch com.example.suite://editor?file=document.txt

# Launch viewer
nah launch com.example.suite://viewer

# Launch converter (hidden component)
nah launch com.example.suite://converter input.txt output.md
```

## Component Environment Variables

When launched via `nah launch`, components receive:

- `NAH_COMPONENT_ID` - Component identifier (e.g., "editor")
- `NAH_COMPONENT_URI` - Full URI that was used to launch
- `NAH_COMPONENT_PATH` - Path portion of URI
- `NAH_COMPONENT_QUERY` - Query string (if present)
- `NAH_COMPONENT_FRAGMENT` - Fragment (if present)
- `NAH_COMPONENT_REFERRER` - URI of calling component (if present)

Components can use these to implement deep linking and inter-component communication.

## Testing Component Features

1. **Query Parameters**: Launch editor with a file parameter
   ```bash
   nah launch "com.example.suite://editor?file=test.txt"
   ```

2. **Component Discovery**: List all components
   ```bash
   nah components --json
   ```

3. **Hidden Components**: Converter won't show in normal UI but can be launched
   ```bash
   nah launch com.example.suite://converter
   ```

4. **Referrer Tracking**: Launch viewer, then editor (viewer logs the referrer)
   ```bash
   nah launch com.example.suite://viewer --referrer com.example.suite://editor
   ```
