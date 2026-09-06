# How NAH works

NAH turns local package and host metadata into one launch contract. That contract contains the executable, argument vector, working directory, environment, library paths, permission requests, artifact digests, and trust state.

## Lifecycle

1. `nah pack` creates a deterministic `.nap` or `.nak` archive from one manifest and its local files.
2. `nah install` validates and stages the package, then atomically installs its payload and registry record.
3. App installation selects the highest installed NAK version matching the app requirement and pins its exact record.
4. `nah show` composes the pinned records and host configuration without executing anything.
5. `nah run` executes that contract directly, without a shell.

Selection happens at installation time. Adding a newer NAK does not silently change an existing app.

## Composition inputs

Composition uses only explicit inputs:

- the app manifest;
- its install record;
- the pinned NAK record, if any;
- `<root>/host/host.json`;
- composition options and allowed overrides.

Environment operations are applied from app, NAK, host, then install overrides. Supported operations are `set`, `prepend`, `append`, and `unset`. Placeholder expansion happens afterward. A missing placeholder is an error; composition does not read arbitrary process environment variables as a fallback.

Relative app and NAK paths resolve beneath their respective installation roots. Escaping paths, including escapes through symlinked parents, are rejected.

## Ownership boundary

NAH owns local packaging, installation records, deterministic composition, inspection, and optional execution. The embedding host owns acquisition, artifact verification, authorization, sandboxing, process supervision, and enforcement of declared permissions.

See [the specification](../SPEC.md) for normative ordering and validation rules.
