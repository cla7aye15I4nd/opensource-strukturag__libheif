# PR: bug13 — `Box_idat::read_data` `wait_for_file_size` signed/unsigned mix

**Branch:** `fix/idat-signed-unsigned-mix` (off `master`)
**Severity:** Low — wrong error, not memory unsafety.
**Files touched:** `libheif/box.cc`

## Summary

```cpp
StreamReader::grow_status status =
    istr->wait_for_file_size((int64_t) m_data_start_pos + start + length);
```

`m_data_start_pos` is cast to `int64_t`, then added to two `uint64_t`
values (`start`, `length`). The result is computed in `int64_t`; if
the unsigned sum exceeds `INT64_MAX`, the implicit narrowing is UB.
The resulting negative `int64_t` then widens back to a huge `uint64_t`
when handed to `wait_for_file_size()` — which returns `size_beyond_eof`
and yields a spurious parse error rather than a real bound.

UBSAN with `-fsanitize=integer` flags this as an
implicit-conversion error.

## Fix

Reject negative `m_data_start_pos` and compute the target purely in
`uint64_t` with explicit overflow guards:

```cpp
if (m_data_start_pos < 0) { return Error(...); }
uint64_t target_data_start = static_cast<uint64_t>(m_data_start_pos);
if (start > std::numeric_limits<uint64_t>::max() - length ||
    start + length > std::numeric_limits<uint64_t>::max() - target_data_start) {
  return Error(...);
}
StreamReader::grow_status status =
    istr->wait_for_file_size(target_data_start + start + length);
```

## Verification

The bug13 harness chains through bug03's site
(`bitstream.cc` `wait_for_available_bytes`) which is fixed by the
`fix/wait-for-available-bytes-overflow` PR. With both PRs applied the
reproducer exercises the cleaned-up arithmetic and exits without
UBSAN diagnostics; in isolation only the bug03 site fires.

## PR title

`box: compute Box_idat::read_data wait_for_file_size target as uint64_t`

## PR body

```
`(int64_t) m_data_start_pos + start + length` mixes int64_t with
two uint64_t values; the result is computed in int64_t, and if the
unsigned sum exceeds INT64_MAX the implicit narrowing is UB. The
negative int64_t widens back to a huge uint64_t for
wait_for_file_size, masking what should be an OOB result with a
spurious size_beyond_eof.

Reject negative m_data_start_pos and compute the target purely in
uint64_t with explicit overflow guards.
```
