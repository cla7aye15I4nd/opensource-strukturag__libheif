# PR: bug17 — `meta_box_start + box_size` overflow in `FileLayout::read`

**Branch:** `fix/file-layout-meta-box-overflow` (off `master`)
**Severity:** **High** — chains directly into bug02
(`BitstreamRange` `end < start` underflow).
**Files touched:** `libheif/file_layout.cc`

## Summary

```cpp
uint64_t end_of_meta_box = meta_box_start + box_header.get_box_size();
```

The on-disk `largesize` field is attacker-controlled. With
`meta_box_start = N` (small) and `largesize = UINT64_MAX - N + k` (k
small), the addition wraps to `k`. The subsequent
`BitstreamRange(meta_box_start, end_of_meta_box)` is then called
with `end < start`; under `-DNDEBUG` the constructor's
`assert(end >= start)` is a no-op and `m_remaining` underflows to
`~SIZE_MAX`, disabling all `prepare_read()` bounds checks for the
remainder of the meta-box parse.

This is the source half of bug02's "highest-severity consequence";
the sink half is fixed in `fix/bitstreamrange-end-lt-start`.

## Fix

Reject the wrapping addition before constructing `BitstreamRange`
(also covers the `mini` and `moov` branches in the same file):

```cpp
if (box_header.get_box_size() > std::numeric_limits<uint64_t>::max() - meta_box_start) {
  return {heif_error_Invalid_input,
          heif_suberror_No_meta_box,
          "meta box size too large, integer overflow"};
}
```

(Plus `#include <limits>` if not already present.)

## Verification

`audit_repros/bug17_file_layout_meta_box_overflow/` constructs the
wrapping meta box. The libheif code path is now correct: it rejects
the input with the new error. The harness's own deliberate-UB setup
arithmetic still trips UBSAN (it is the harness simulating the
broken upstream condition); with both this PR and
`fix/bitstreamrange-end-lt-start` applied, the BitstreamRange call
site is also UB-free.

## PR title

`file_layout: reject meta_box_start + box_size overflow`

## PR body

```
A crafted largesize on the meta box can drive
meta_box_start + box_size past UINT64_MAX. The wrap produces
end_of_meta_box < meta_box_start; BitstreamRange(start, end) is
then called with end < start and (under NDEBUG) m_remaining
underflows to ~SIZE_MAX, disabling bounds checks for the rest of
the meta-box parse.

Reject the addition with heif_suberror_No_meta_box before
constructing BitstreamRange. Pairs with the BitstreamRange-side
fix for defence in depth.
```
