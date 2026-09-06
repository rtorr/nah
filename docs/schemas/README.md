# JSON schemas

These schemas document NAH's JSON boundaries. JSON is used only where humans,
build tools, persisted state, or another process cross the typed C++ API. The
schemas support editors, build-time validation, and independent producers and
consumers; they are not a second domain model.

NAH does not embed a JSON Schema engine. Its typed parsers reject incompatible
versions and invalid semantic inputs. Producers that need complete structural
validation should validate against these files before calling NAH.

| Schema | Artifact | Owner |
| --- | --- | --- |
| `nap.v2.json` | package-root `nap.json` | app developer |
| `nak.v1.json` | package-root `nak.json` | NAK developer |
| `nah.v2.json` | `<root>/host/host.json` | host |
| `launch.v2.json` | serialized launch contract | `nah show` / host API |
| `app-record.v2.json` | `<root>/registry/apps/*.json` | `nah install` |
| `nak-record.v1.json` | `<root>/registry/naks/*.json` | `nah install` |

The three package/host files are authored inputs. The launch contract is public
machine output. Registry records are implementation-owned persisted state; use
the NAH API to read them instead of depending on their files directly.

Canonical URLs use `https://nah.rtorr.com/schemas/<filename>`. Add the relevant URL as `$schema` for editor validation.

Environment maps accept strings or `{ "op", "value", "separator" }` objects. Operations are `set`, `prepend`, `append`, and `unset`; `value` is not used by `unset`.

Schema IDs may gain optional fields. A breaking shape requires a new schema ID.
[`SPEC.md`](../../SPEC.md) defines behavior beyond structural validation.
