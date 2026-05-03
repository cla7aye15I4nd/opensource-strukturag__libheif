# PR: bug05 — iloc `base_offset_size` not validated against `{0, 4, 8}`

**Branch:** `fix/iloc-base-offset-size-validation` (off `master`)
**Severity:** **High** — specific case of bug04.
**Files touched:** `libheif/box.cc`

## Summary

`base_offset_size` is a 4-bit field. ISO/IEC 14496-12 §8.11.3 allows
only 0, 4 or 8. The current code:

```cpp
item.base_offset = 0;
if (base_offset_size == 4)      item.base_offset = read32();
else if (base_offset_size == 8) item.base_offset = (read32()<<32)|read32();
// no else — and no error if base_offset_size is e.g. 2
```

With `base_offset_size = 2`, `item.base_offset` stays 0, the 2-byte
field is **not consumed**, and the next `read16()` for
`extent_count` picks up those 2 bytes — yielding an
attacker-controlled extent count.

## Fix

Validate all four iloc field-size nibbles up-front against
`{0, 4, 8}` and return `heif_suberror_Invalid_box_size` on any other
value:

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

This is the same one-shot check shipped by `fix/iloc-field-size-validation`
(umbrella) and the other per-nibble PRs.

## Verification

```
[bug05] read_from_memory err="iloc field size must be 0, 4 or 8"
```

(Before: ASAN heap-buffer-overflow on the harness's downstream
consumer.)

## PR title

`box: reject iloc with base_offset_size not in {0, 4, 8}`

## PR body

```
Box_iloc::parse only handles base_offset_size in {0, 4, 8}; any
other nibble silently leaves base_offset = 0 and the field bytes
unconsumed, so the next read16() for extent_count picks them up.
With an attacker-chosen base_offset_size = 2 this yields a wrong
extent count and a downstream OOB on the iloc consumer.

Validate all four iloc field-size nibbles up-front against
{0, 4, 8}.
```
