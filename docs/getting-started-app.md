# App packages

Create a standalone app scaffold:

```bash
nah init --app --id com.example.app ./app
```

The generated `nap.json` uses the current shape:

```json
{
  "$schema": "https://nah.rtorr.com/schemas/nap.v2.json",
  "app": {
    "identity": {"id": "com.example.app", "version": "0.1.0"},
    "execution": {"entrypoint": "bin/app"}
  }
}
```

The entrypoint must be a regular file inside the package. To request an installed runtime, add `nak_id` and `nak_version_req` under `app.identity`; add an optional loader name under `app.execution.loader`.

Package and test it in an isolated root:

```bash
nah pack ./app --output app.nap
nah --root ./test-root install app.nap
nah --root ./test-root show com.example.app
nah --root ./test-root run com.example.app -- arg1
```

See the [app schema](schemas/nap.v2.json) for optional environment, layout, metadata, and permission declarations.
