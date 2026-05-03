# PR: bug02 — `BitstreamRange(start, end)` underflow when `end < start`

**Branch:** `fix/bitstreamrange-end-lt-start` (off `master`)
**Severity:** **High** — chained sink for the `file_layout.cc` overflow
sites (bug17/bug18). The audit comment names this as the
highest-severity consequence of those bugs.
**Files touched:** `libheif/bitstream.cc`

## Summary

`BitstreamRange::BitstreamRange(istr, start, end)` initialises
`m_remaining = end - start` and then calls `assert(end >= start)`.
Under `-DNDEBUG` (release builds, including the audit_repros build)
the assert is compiled out. If a caller passes `end < start` — readily
producible via the `meta_box_start + box_size` overflow in
`file_layout.cc` — the subtraction wraps to `~SIZE_MAX`. Every
subsequent `prepare_read(N)` then sees `m_remaining >= N` trivially and
all bounds checks are bypassed for the rest of the box parse.

## Fix

Replace the assert with a real runtime check: when `end < start`,
initialise `m_remaining = 0` and set `m_error = true`. Read paths then
short-circuit safely and `get_error()` returns `End_of_data`. Apply
the same treatment to a failed `seek()`.

```cpp
BitstreamRange::BitstreamRange(std::shared_ptr<StreamReader> istr,
                               size_t start, size_t end)
  : m_istr(std::move(istr)), m_remaining(end >= start ? end - start : 0)
{
  if (end < start) {
    m_error = true;
    return;
  }
  if (!m_istr->seek(start)) {
    m_error = true;
  }
}
```

## Verification

The reproducer (`audit_repros/bug02_bitstreamrange_end_lt_start/`)
constructs a meta box whose `largesize` plus `meta_box_start` wraps
past `UINT64_MAX`, then walks the resulting range with `read8()` 256
times. Before the fix: ASAN heap-buffer-overflow. After:

```
[bug02] survived OOB walk, sink=0x00
```

Every `read8()` short-circuits via `prepare_read()` returning false;
no OOB read.

## PR title

`bitstream: turn BitstreamRange end<start underflow into a hard error`

## PR body

```
BitstreamRange(istr, start, end) initialised m_remaining as
`end - start` and relied on assert(end >= start). Under NDEBUG the
assert is a no-op, the subtraction wraps to ~SIZE_MAX, and every
prepare_read() trivially succeeds — all bounds checks are bypassed
for the rest of the range.

This is the chained sink for the file_layout.cc overflow sites
(meta/moov box_start + box_size). Replace the assert with a real
check: on end < start, set m_remaining = 0 and m_error = true so
read paths short-circuit and get_error() reports End_of_data. Apply
the same to seek() failure.
```
