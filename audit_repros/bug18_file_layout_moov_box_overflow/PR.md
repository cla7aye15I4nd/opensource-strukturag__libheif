# PR: bug18 — `moov_box_start + box_size` overflow in `FileLayout::read`

**Branch:** `fix/file-layout-moov-box-overflow` (off `master`)
**Severity:** **High** — same primitive as bug17, on the `moov`
branch (sequences / track parsing).
**Files touched:** `libheif/file_layout.cc`

## Summary

```cpp
uint64_t end_of_moov_box = moov_box_start + box_header.get_box_size();
```

A crafted `largesize` on the `moov` box wraps the addition to a
small value. The subsequent
`BitstreamRange(moov_box_start, end_of_moov_box)` is then called
with `end < start`; under `-DNDEBUG` the constructor's
`assert(end >= start)` is a no-op and `m_remaining` underflows to
`~SIZE_MAX`, disabling all bounds checks for the moov-box parse.

This exercises the sequences / track-parsing path; bug17 is the
same primitive on the still-image meta path. The downstream sink is
bug02 (BitstreamRange `end < start`), addressed by its own PR.

## Fix

Reject the wrapping addition before constructing `BitstreamRange`:

```cpp
if (box_header.get_box_size() > std::numeric_limits<uint64_t>::max() - moov_box_start) {
  return {heif_error_Invalid_input,
          heif_suberror_No_moov_box,
          "moov box size too large, integer overflow"};
}
```

(Plus `#include <limits>` if not already present.)

## Verification

`audit_repros/bug18_file_layout_moov_box_overflow/`. As with bug17,
the harness exercises the wrap *itself* (it computes
`moov_box_start + crafted_box_size` in user code), so under
`-fsanitize=integer` UBSAN flags the harness's own setup
arithmetic. libheif's new check is correct: any input that triggers
this overflow is rejected before `BitstreamRange` is constructed.

## PR title

`file_layout: reject moov_box_start + box_size overflow`

## PR body

```
A crafted largesize on the moov box drives
moov_box_start + box_size past UINT64_MAX. The wrap produces
end_of_moov_box < moov_box_start; BitstreamRange(start, end) is
then called with end < start and (under NDEBUG) m_remaining
underflows to ~SIZE_MAX, disabling bounds checks for the rest of
the moov-box parse.

Reject the addition with heif_suberror_No_moov_box before
constructing BitstreamRange. Same primitive as bug17 in the meta
branch.
```
