# Host integration

A host owns the NAH root and policy around the launch contracts it executes. Choose the root explicitly in deployments:

```bash
nah --root /opt/my-product/nah install runtime.nak
nah --root /opt/my-product/nah install app.nap
nah --root /opt/my-product/nah show com.example.app
```

Create a root explicitly, or let the first installation create it:

```bash
nah init --host ./nah-root
```

The root contains managed `apps`, `naks`, `registry`, `host`, and `staging` directories. Its optional host environment is stored at `<root>/host/host.json`:

```json
{
  "environment": {
    "DEPLOYMENT_ENV": "production"
  },
  "paths": {
    "library_prepend": [],
    "library_append": []
  },
  "overrides": {
    "allow_env_overrides": false,
    "allowed_env_keys": []
  }
}
```

The command creates local state only; it does not fetch or install packages. Remote acquisition, artifact verification, signature policy, permission enforcement, and sandboxing remain responsibilities of the surrounding host or deployment system.

Embed [`NahHost`](../include/nah/nah_host.h) when a product needs to inspect contracts, provide UI, or execute apps programmatically.
