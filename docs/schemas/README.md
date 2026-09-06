# JSON schemas

These schemas define the current authored and generated JSON formats:

| Schema | Artifact | Owner |
| --- | --- | --- |
| `nap.v1.json` | package-root `nap.json` | app developer |
| `nak.v1.json` | package-root `nak.json` | NAK developer |
| `nah.v1.json` | `<root>/host/host.json` | host |
| `app-record.v1.json` | `<root>/registry/apps/*.json` | `nah install` |
| `nak-record.v1.json` | `<root>/registry/naks/*.json` | `nah install` |

Canonical URLs use `https://nah.rtorr.com/schemas/<filename>`. Add the relevant URL as `$schema` for editor validation.

Environment maps accept strings or `{ "op", "value", "separator" }` objects. Operations are `set`, `prepend`, `append`, and `unset`; `value` is not used by `unset`.

Schema ids ending in `.v1` may gain optional fields. A breaking shape requires a new schema id. [`SPEC.md`](../../SPEC.md) defines behavior beyond structural validation.
