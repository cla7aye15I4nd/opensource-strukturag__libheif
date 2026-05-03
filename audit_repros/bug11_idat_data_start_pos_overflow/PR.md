# PR: bug11 — `Box_idat::read_data` `m_data_start_pos + get_box_size()` overflow

**Branch:** `fix/idat-data-start-pos-overflow` (off `master`)
**Severity:** Latent (defense-in-depth).
**Files touched:** `libheif/box.cc`

## Summary

```cpp
if (start > (uint64_t) m_data_start_pos + get_box_size()) { ... }
```

`m_data_start_pos` is `std::streampos` (effectively `int64_t`). Two
issues:

1. The cast to `uint64_t` is well-defined only for non-negative
   values.
2. The addition with `get_box_size()` (also `uint64_t`) can wrap if
   the idat box would extend past the top of the address space.

## Fix

Reject negative `m_data_start_pos` and guard the addition:

```cpp
if (m_data_start_pos < 0) {
  return Error(heif_error_Invalid_input, heif_suberror_End_of_data);
}
uint64_t data_start = static_cast<uint64_t>(m_data_start_pos);
uint64_t box_size = get_box_size();
if (box_size > std::numeric_limits<uint64_t>::max() - data_start) {
  return Error(heif_error_Invalid_input, heif_suberror_End_of_data);
}
if (start > data_start + box_size) { ... }
```

`start + length > box_size` is left in place — `start` and `length`
are each bounded by `MAX_FILE_POS = 2^55-1`, so the sum fits in
`uint64_t` (proven by the bug12 invariant).

## Verification

The reproducer drives a hand-forged idat through a
`LyingPositionReader` returning a position near `UINT64_MAX`. The
harness today also chains through bug03 / bug13 sites; with this fix
plus those PRs applied the harness terminates with
`Invalid_input / End_of_data`. The libheif code change here is
correct in isolation.

## PR title

`box: guard Box_idat::read_data start/length bounds against overflow`

## PR body

```
`(uint64_t) m_data_start_pos + get_box_size()` casts std::streampos
to uint64_t and adds get_box_size(). The cast is well-defined only
for non-negative values, and the addition can wrap if the box ends
near UINT64_MAX.

Reject negative m_data_start_pos explicitly and check
data_start + box_size for overflow before the comparison.
```
