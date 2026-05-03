# PR: bug01 — guard `StreamReader_memory::read` against `uint64_t` addition wrap

**Branch:** `fix/streamreader-memory-read-overflow` (off `master`)
**Severity:** Latent (invariant maintained today)
**Files touched:** `libheif/bitstream.cc`

## Summary

`StreamReader_memory::read()` computes `end_pos = m_position + size` and
then bounds-checks `end_pos > m_length`. When `m_position` is near
`UINT64_MAX` and `size` is large, the unsigned addition wraps to a
small value, the bounds check passes, and `memcpy` reads past the end
of `m_data`.

The current code path keeps `m_position <= m_length` via `seek()`, so
the wrap is unreachable from the public API. But the bounds check
*assumes* that invariant — a regression in `seek()`, or any future
caller that bypasses it, immediately exposes the OOB read.

## Fix

Add an explicit overflow guard before the addition (plus
`#include <limits>`):

```cpp
if (size > std::numeric_limits<uint64_t>::max() - m_position) {
  return false;
}
```

## Verification

The reproducer (`audit_repros/bug01_streamreader_memory_read_overflow/`)
forges `m_position = UINT64_MAX - 4`, `m_length = UINT64_MAX - 4` via a
test shim, then calls `read(buf, 200)`. Before the fix UBSAN reports
an unsigned-integer-overflow at the addition and ASAN would catch the
OOB `memcpy`. After the fix:

```
[bug01] seek to high offset: 1
[bug01] read past wrap: 0 (ASAN should fire here)
```

`read()` returns `false` cleanly; no UBSAN/ASAN diagnostic.

## PR title

`bitstream: guard StreamReader_memory::read against uint64 addition wrap`

## PR body

```
StreamReader_memory::read() computes m_position + size before the
bounds check; if m_position is near UINT64_MAX the addition wraps
and the check passes a request that would memcpy past the end of
m_data.

Today the wrap is unreachable because seek() rejects positions
beyond m_length, but the bounds check should not silently rely on
that invariant. Add an explicit overflow guard so the function is
correct in isolation.

No behaviour change on well-formed input.
```
