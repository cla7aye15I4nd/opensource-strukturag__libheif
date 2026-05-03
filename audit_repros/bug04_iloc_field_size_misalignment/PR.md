# PR: bug04 — iloc field-size validation against `{0, 4, 8}` (umbrella)

**Branch:** `fix/iloc-field-size-validation` (off `master`)
**Severity:** **High** — stream-misalignment → codec data injection.
**Files touched:** `libheif/box.cc`

## Summary

The iloc header packs four nibbles (`offset_size`, `length_size`,
`base_offset_size`, and for v1/v2 `index_size`). ISO/IEC 14496-12
§8.11.3 requires each of these to be 0, 4 or 8. The per-extent read
loops dispatch on those three values via:

```cpp
if (offset_size == 4)      extent.offset = read32();
else if (offset_size == 8) extent.offset = (read32()<<32) | read32();
// no else
```

Any other nibble silently falls through *without consuming the field
bytes from the stream*. The next field read then picks up the
unconsumed bytes — for example with `offset_size = 5,
length_size = 4`, the 5 offset bytes remain in the stream, the
read32() for `extent.length` consumes the first 4, yielding an
attacker-chosen length while `extent.offset` stays 0. With
`extent.offset = 0` the consumer seeks to byte 0 of the file and
ships `length` bytes (arbitrary HEIF prefix data) to the image
codec — bypassing all HEIF-level data validation.

This is the umbrella PR for the primitive. The per-nibble PRs
(`fix/iloc-base-offset-size-validation`, `fix/iloc-offset-size-validation`,
`fix/iloc-length-size-validation`) ship the same patch scoped to
their respective audit comment.

## Fix

Validate every field-size nibble before any extent is read:

```cpp
auto valid_field_size = [](int s) { return s == 0 || s == 4 || s == 8; };
if (!valid_field_size(offset_size) ||
    !valid_field_size(length_size) ||
    !valid_field_size(base_offset_size) ||
    !valid_field_size(index_size)) {
  return {heif_error_Invalid_input,
          heif_suberror_Invalid_box_size,
          "iloc field size must be 0, 4 or 8"};
}
```

(Placed after `index_size` is read so v1/v2 can be checked too.)

## Verification

| Repro | Before | After |
|-------|--------|-------|
| bug04 (all four invalid) | parse-success / silent misalignment | `iloc field size must be 0, 4 or 8` |
| bug05 (`base_offset_size=2`) | ASAN heap-buffer-overflow downstream | same error |
| bug06 (`offset_size=5`)      | ASAN heap-buffer-overflow downstream | same error |
| bug07 (`length_size=7`)      | ASAN heap-buffer-overflow downstream | same error |

`audit_repros/bug04_iloc_field_size_misalignment/` post-fix:

```
[bug04] read_from_memory err="iloc field size must be 0, 4 or 8"
```

## PR title

`box: reject iloc with field sizes outside {0, 4, 8}`

## PR body

```
ISO/IEC 14496-12 §8.11.3 requires offset_size, length_size,
base_offset_size and (v1/v2) index_size each to be 0, 4 or 8. The
per-extent read loops in Box_iloc::parse only handle those three
values; any other nibble silently falls through without consuming
the field bytes, so the next field read picks them up and yields
attacker-controlled offsets/lengths. The audit_repros harnesses
demonstrate the resulting downstream OOB read.

Reject any field size outside {0, 4, 8} with
heif_suberror_Invalid_box_size before any extent is parsed.
```
