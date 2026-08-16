# Pfhorge Native vNext — Draft 0.1

Status: **FORMAT-1A foundation draft**  
Native extension: **`.pfhlev` remains the Pfhorge Level extension**  
Scenario extension: **`.sen` remains the current Pfhorge Scenario extension for compatibility**  
Physical container: ZIP  
Semantic representation: UTF-8 JSON  
Provisional package media type: `application/vnd.pfhorge.package+zip`

> This draft intentionally freezes the package/container rules before freezing every
> Marathon/Pfhorge semantic field. The next format phase maps the complete legacy
> `LELevelData` object graph and every supported external codec into the canonical
> semantic model. The package rules are designed so that model can grow without
> replacing the container again.

## 1. Design goals

Pfhorge Native vNext is the lossless, extensible superset representation used by
Pfhorge itself. It is not constrained to the limitations of Marathon 1, Marathon 2,
Marathon Infinity, Aleph One, Pathways Into Darkness, or third-party editors.

The format is designed around these principles:

1. **Open by construction.** A `.pfhlev` can be copied to `.zip` or opened by any
   normal ZIP utility. Its authoritative semantic data is plain UTF-8 JSON.
2. **One user-facing file.** ZIP is only the package/binder. Users still see a
   Pfhorge document with a Pfhorge icon and `.pfhlev`/`.sen` identity.
3. **No Cocoa object archive as the specification.** `NSKeyedArchiver` remains a
   legacy codec only.
4. **Canonical semantics are richer than any target codec.** External formats are
   read/written through explicit codecs and capability analysis.
5. **Stable object identity.** Native entities use RFC 9562 UUID strings instead of
   transient array indexes as identity. External array indexes are codec/provenance
   data.
6. **Unknown data survives when it can survive safely.** Opaque source bytes carry
   explicit preservation rules; Pfhorge never calls an unknown chunk "round-trip
   safe" simply because it copied the bytes.
7. **Authoritative data is separated from derived state.** Portal graphs,
   triangulation, renderer caches, current media height, thumbnails, and similar
   regenerated state are not authoritative level semantics.
8. **Deterministic and diffable.** Semantic JSON has stable formatting and ordering.
   Package writers use deterministic entry ordering and metadata.
9. **Safe failure.** Unknown required extensions, invalid references, malformed ZIP
   paths, decompression bombs, and unsupported target semantics must fail or warn
   explicitly rather than silently corrupting a document.
10. **Old Pfhorge files remain readable.** Legacy `.pfhlev` is a codec, not something
    that is overwritten in place without an explicit migration/save operation.

## 2. Why ZIP exists

ZIP is not the semantic format and compression is not the main reason it is used.

A logical Pfhorge document is a group of related resources:

```text
My Level/
    mimetype
    manifest.json
    document.json
    levels/<uuid>.json
    extensions/...
    provenance/...
    opaque/...
```

The packed `.pfhlev` is simply the standard ZIP manifestation of that logical
directory. This permits JSON, an original Marathon WAD, an unknown binary chunk,
an image, or a future extension to remain in its natural representation while the
user handles one document file.

A conforming tool MAY expose an unpacked directory representation for development
and archival workflows.

## 3. Package layout

Minimum single-level package:

```text
Example.pfhlev
├── mimetype
├── manifest.json
├── document.json
└── levels/
    └── 7b69c5c9-5738-42e2-bbda-090075b18456.json
```

Optional content:

```text
extensions/
provenance/
    sources.json
    original/
opaque/
    index.json
cache/
```

`cache/`, if present in a future revision, MUST be disposable without changing the
meaning of the document.

## 4. ZIP profile

Pfhorge Native vNext uses the PKWARE ZIP format with a deliberately small profile.

### 4.1 Required writer behavior

A native writer:

- MUST write `mimetype` as the first ZIP entry.
- MUST store `mimetype` uncompressed.
- MUST give `mimetype` no ZIP extra field.
- MUST use UTF-8 file names and `/` as the path separator.
- MUST NOT write absolute paths, `.` or `..` path components, or backslashes.
- MUST NOT write symbolic links.
- MUST NOT use ZIP encryption.
- MUST NOT create split/spanned archives.
- MUST use only ZIP method 0 (Store) or method 8 (Deflate).
- SHOULD use Store by default. Pfhorge documents are small; uncompressed packages
  maximize deterministic output, transparent recovery, and implementation simplicity.
- MUST sort all entries after `mimetype` by package path when writing a deterministic
  package.
- SHOULD normalize non-semantic ZIP timestamps/permissions rather than treating them
  as document metadata.

A reader MUST support Store and Deflate. This means a power user may unpack and
repack a Pfhorge document with ordinary ZIP software without being forced to know
Pfhorge's writer preference.

ZIP64 is **not required by FORMAT-1A writers**. Readers SHOULD reject a package that
exceeds configured safety limits before allocating/extracting it. Whether ZIP64
becomes mandatory reader support is intentionally deferred until asset/resource
limits are finalized.

### 4.2 `mimetype`

`mimetype` is US-ASCII with no BOM, whitespace, or newline:

```text
application/vnd.pfhorge.package+zip
```

The string is provisional until the public media-type policy is frozen. The
package's `manifest.json` is the normative source for Pfhorge kind/version.

The "first uncompressed mimetype entry" convention follows the same engineering
pattern used by EPUB and OpenDocument: it permits type identification without
extracting the whole archive.

## 5. JSON profile

All normative JSON:

- MUST be UTF-8.
- MUST NOT contain a byte-order mark.
- MUST NOT contain duplicate object member names.
- MUST NOT contain NaN or Infinity.
- MUST keep integer JSON numbers within the interoperable I-JSON safe range
  `[-9007199254740991, 9007199254740991]`.
- MUST encode exact larger integers as schema-defined decimal strings.
- MUST preserve Unicode strings as supplied; writers MUST NOT silently normalize
  Unicode text merely for serialization.
- SHOULD use two-space indentation, sorted object keys, and a final LF for
  human-facing files.
- MUST treat array order as semantic only where the schema says it is semantic.
- MUST use UUID strings in canonical lowercase RFC 9562 textual form for stable
  entity identities.

The project intentionally does **not** require RFC 8785 JCS byte serialization for
the human-readable source files. JCS is excellent for hashing/signatures but emits
no insignificant whitespace. Pfhorge instead hashes the exact package-resource
bytes listed in `manifest.json`. A future signature extension MAY define JCS
canonicalization for signed JSON values.

## 6. Manifest

`manifest.json` identifies the package and hashes all normative resources except
itself and `mimetype`.

Core fields:

```json
{
  "$schema": "urn:pfhorge:schema:manifest:1",
  "format": "org.pfhorge.native",
  "formatVersion": "1.0.0-draft.1",
  "kind": "level",
  "document": "document.json",
  "extensions": [],
  "resources": [
    {
      "mediaType": "application/json",
      "path": "document.json",
      "sha256": "..."
    }
  ]
}
```

`resources` is a corruption/integrity inventory, not a cryptographic signature.
A conforming validator MUST verify hashes before trusting a normative resource.

## 7. Document and levels

`document.json` contains stable document identity and the ordered level roster.
A single `.pfhlev` normally contains exactly one level. A scenario contains more.

The level payload in FORMAT-1A establishes the durable section boundaries:

```text
metadata
geometry
surfaces
world
terminals
editor
extensions
provenance
```

FORMAT-1B will freeze the complete field-by-field canonical semantics after an
audit of:

- every property persisted by `LELevelData` and its child classes,
- every Marathon/Aleph One map chunk Pfhorge reads/writes,
- Pathways conversion data,
- current Map Intake provenance/loss-ledger data,
- editor-only layers/names/groups/settings,
- renderer-derived values that must *not* become authoritative.

Until that audit lands, `level.schema.json` deliberately provides strict top-level
sectioning but permits section-local draft properties. This avoids accidentally
making a twenty-year-old Objective-C implementation detail part of the new public
format before it has been classified.

## 8. Stable identities and external indexes

Native identity is UUID-based.

Example:

```json
{
  "id": "8a940bdd-3a2c-4df7-b093-eac0b2272f13",
  "startPoint": "e1e15965-7e9a-47b1-b920-adab46dad9ba",
  "endPoint": "fb3c0ab5-475c-41f9-81a0-f2fcb2ba724e"
}
```

A Marathon codec may assign these objects array indexes while encoding:

```text
UUID 8a940bdd... -> LINS[225]
```

The source index can also be retained as provenance. Reordering native arrays MUST
NOT change entity identity.

## 9. Extensions

Extension identifiers are reverse-DNS names such as:

```text
org.pfhorge.editor.layers
org.pfhorge.visual-mode
org.alephone.example-feature
```

Each declaration specifies at least:

- `id`
- `version`
- `requiredForRead`
- `requiredForWrite`

Rules:

- Unknown extension with `requiredForRead=true`: the consumer MUST NOT claim a
  faithful semantic load.
- Unknown extension with `requiredForWrite=true`: the consumer MUST NOT overwrite
  the document as though it could preserve the extension.
- Unknown optional extensions SHOULD be preserved byte-for-byte where their
  preservation rule allows it.

Extensions may use JSON or binary payload resources referenced by package path.

## 10. Provenance and opaque source data

Pfhorge already has a revival-era Map Intake snapshot and loss ledger. vNext does
not throw this work away; it absorbs it into the document model.

An opaque source fragment records, at minimum:

```text
source format / codec
source resource or tag
ordinal/location when known
raw package path
SHA-256
preservation class
```

Preservation classes:

- `safe_passthrough`
- `safe_if_structure_unchanged`
- `requires_codec_support`
- `invalidated`

The original complete source may optionally be embedded under
`provenance/original/`. Embedding source bytes is archival provenance, not an
excuse to claim the editable semantic model is lossless.

## 11. Operation model

The same codec/canonical-model architecture powers all operations.

### Open

Read a supported format as a document and remember its active codec. No
`unionLevel:`-style geometry merge is involved.

### Save

Write through the document's active codec. If current semantics exceed that
codec's capabilities, Save MUST refuse or require an explicit conversion policy.

### Save As

Run capability analysis, convert through the selected target codec, validate the
result, and make that codec the document's new active format after success.

### Import

Bring complete level(s) from another document into the current project/scenario
while preserving source level-wide semantics such as environment metadata.

### Export

Write a target-format copy without changing the active document format. Report
all transformations and losses.

### Merge

Merge selected content into an existing level. Merge computes dependency closure
and ID/reference remapping. Destination level-wide metadata normally remains
authoritative. Environment/texture conflicts are explicit merge policy choices.

This distinction directly fixes the legacy failure where Pfhorge's
"Combine/Import" operation copied Lava geometry/media into a destination level
while leaving the destination Pfhor `Minf` metadata intact.

## 12. Capability and loss analysis

A codec advertises capabilities at field/feature granularity, not as one boolean.

Every conversion produces a report such as:

```text
Target: Marathon 2 Map

geometry                 lossless
textures                 lossless
media                    lossless
lights                   lossless
platforms                lossless
editor layers             editor-only / omitted
future extension foo      unsupported

Overall: lossy unless foo is removed or converted
```

No target writer may silently discard unsupported canonical semantics.

## 13. Validation layers

Validation occurs at four layers:

1. **Container:** ZIP structure, path safety, entry limits, compression profile.
2. **Resource:** UTF-8/JSON correctness and SHA-256 resource hashes.
3. **Schema:** JSON Schema Draft 2020-12.
4. **Semantic:** referential integrity, topology, codec constraints, extension
   requirements, and format-specific invariants.

## 14. Round-trip contracts

Tests distinguish:

- **byte-preserving**
- **semantic-lossless**
- **normalized-lossless**
- **lossy with explicit policy**

For writable codecs:

```text
fixture -> decode -> canonical -> encode -> decode -> semantic compare
```

must be automated in CI.

## 15. Legacy `.pfhlev`

The existing ten-byte-header + `NSKeyedArchiver` `.pfhlev` remains supported as
`LegacyPfhorgeLevelCodec`.

A vNext `.pfhlev` is distinguished by ZIP signature plus the Pfhorge `mimetype`.
An old file is distinguished by the legacy Pfhorge signatures.

Opening an old file MUST NOT silently overwrite it with vNext merely because it was
opened. Migration is an explicit Save/Save As decision until compatibility policy
is mature.

## 16. Security limits

Readers MUST apply configurable limits before extraction/allocation. FORMAT-1A's
reference validator defaults to:

- max 4096 entries
- max 64 MiB uncompressed per entry
- max 256 MiB uncompressed package total
- no encrypted entries
- no symbolic links
- no absolute or parent-traversal paths
- no duplicate package paths

These defaults are safety policy, not inherent semantic limits, and can be raised
by a future documented profile.

## 17. Standards/references

Primary references used for this draft:

- RFC 8259 — The JavaScript Object Notation (JSON) Data Interchange Format
  https://www.rfc-editor.org/rfc/rfc8259
- RFC 7493 — The I-JSON Message Format
  https://www.rfc-editor.org/rfc/rfc7493
- RFC 8785 — JSON Canonicalization Scheme
  https://www.rfc-editor.org/rfc/rfc8785
- RFC 9562 — Universally Unique IDentifiers (UUIDs)
  https://www.rfc-editor.org/rfc/rfc9562
- JSON Schema Draft 2020-12
  https://json-schema.org/draft/2020-12
- PKWARE APPNOTE — ZIP File Format Specification
  https://support.pkware.com/pkzip/appnote
- W3C EPUB Open Container Format / EPUB 3
  https://www.w3.org/TR/epub-34/
- OASIS OpenDocument 1.3 Part 2 — Packages
  https://docs.oasis-open.org/office/OpenDocument/v1.3/OpenDocument-v1.3-part2-packages.html
- IANA Structured Syntax Suffix Registry (`+zip`)
  https://www.iana.org/assignments/media-type-structured-suffix/

## 18. FORMAT-1A boundary

FORMAT-1A delivers:

- this package specification draft,
- public JSON schemas for container/document/level envelopes,
- deterministic pack/unpack/inspect/validate reference tooling,
- negative security tests,
- a `Death by accident` regression descriptor,
- a Makefile validation target.

It does **not** yet replace application-side `NSKeyedArchiver` saving. That happens
after FORMAT-1B's complete semantic field audit so the new public format does not
freeze an incomplete model.
