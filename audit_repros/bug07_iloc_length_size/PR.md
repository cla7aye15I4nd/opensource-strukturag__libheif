# PR: bug07 — iloc `length_size` not validated against `{0, 4, 8}`

**Branch:** `fix/iloc-length-size-validation` (off `master`)
**Severity:** **High** — same primitive as bug06, on `length_size`.
**Files touched:** `libheif/box.cc`

## Summary

`length_size` is a 4-bit nibble in the iloc header. ISO/IEC 14496-12
§8.11.3 allows only 0, 4 or 8. With `length_size = 7`:

- `extent.length` stays 0.
- The 7 length bytes remain in the stream.
- The next field read (next extent or next item) consumes them as
  whatever they're typed as, yielding a corrupted parse.

For multi-extent / multi-item iloc this lets an attacker drive the
next field to any value of their choice.

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
[bug07] read_from_memory err="iloc field size must be 0, 4 or 8"
```

(Before: ASAN heap-buffer-overflow on the harness's downstream
consumer.)

## PR title

`box: reject iloc with length_size not in {0, 4, 8}`

## PR body

```
With length_size = 7 the per-extent read for extent.length takes
neither branch and the 7 bytes stay in the stream, corrupting the
next field (next extent or next item).

Validate all four iloc field-size nibbles up-front against
{0, 4, 8} and return heif_suberror_Invalid_box_size on any other
value.
```
