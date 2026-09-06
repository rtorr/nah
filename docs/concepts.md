# Concepts

## App and NAP

An app is described by `nap.json`. Its identity, entrypoint, optional NAK requirement, paths, environment operations, and permission declarations are immutable package metadata.

A `.nap` is the gzip-compressed USTAR archive containing that manifest and its payload.

## NAK

A Native App Kit is a versioned local runtime or SDK described by `nak.json`. It can contribute libraries, resources, environment operations, and named loader executables. Multiple versions may coexist.

An app requests a NAK id and semantic-version range. Installation pins the highest installed match; launches never re-resolve that choice.

## Host

The host owns the NAH root, policy, and process execution. Its optional mutable environment is stored at `<root>/host/host.json`:

```json
{
  "$schema": "https://nah.rtorr.com/schemas/nah.v2.json",
  "environment": {
    "LOG_LEVEL": "info"
  },
  "paths": {
    "library_prepend": [],
    "library_append": []
  }
}
```

## Install records

Registry records are mutable host state. App records identify the installed payload and exact pinned NAK record. NAK records describe their installed paths and loader configuration. Records are treated as untrusted input before filesystem access.

## Launch contract

A launch contract is the deterministic result of composing an app, its install state, the pinned NAK, host state, and explicit options. It is inspectable with `nah show` and executable with `nah run` or `NahHost::executeContract`.

Declared permissions and trust metadata remain evidence for the host. NAH reports them but does not provide a sandbox or authorization system.

See [the specification](../SPEC.md) for exact schemas, ordering, and containment rules.
