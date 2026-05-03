# PR: bug06 — iloc `offset_size` not validated against `{0, 4, 8}`

**Branch:** `fix/iloc-offset-size-validation` (off `master`)
**Severity:** **High** — direct codec-data-injection primitive.
**Files touched:** `libheif/box.cc`

## Summary

`offset_size` is a 4-bit nibble in the iloc header. ISO/IEC 14496-12
§8.11.3 allows only 0, 4 or 8. With `offset_size = 5`:

- The if/else-if branch for `extent.offset` takes neither leg.
- `extent.offset` stays 0.
- The 5 offset bytes are *not* consumed from the stream.
- The next `read32()` for `extent.length` consumes bytes 0..3 of those
  5 — attacker-controlled.
- The consumer (`Box_iloc::read_data`) then seeks to byte 0 of the
  HEIF file and ships `extent.length` bytes (the ftyp prefix etc.) to
  the image codec, bypassing all HEIF-level validation.

## Fix

Validate every iloc field-size nibble against `{0, 4, 8}` up-front:

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

Same single check shipped by `fix/iloc-field-size-validation` and the
other per-nibble PRs.

## Verification

```
read_from_memory: code=2 msg=iloc field size must be 0, 4 or 8
```

(libheif rejects the input. The harness afterwards dereferences the
now-null `iloc` pointer; that's a harness limitation, not a libheif
issue.)

## PR title

`box: reject iloc with offset_size not in {0, 4, 8}`

## PR body

```
With offset_size = 5 the per-extent read for extent.offset takes
neither branch and the field bytes stay unconsumed; the next
read32() for extent.length picks them up. Result: attacker-chosen
length, extent.offset = 0, consumer seeks to byte 0 of the file
and ships arbitrary bytes to the codec.

Validate all four iloc field-size nibbles up-front against
{0, 4, 8} and return heif_suberror_Invalid_box_size on any other
value.
```
