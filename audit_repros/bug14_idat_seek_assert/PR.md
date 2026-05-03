# PR: bug14 — `istr->seek/read` failures in `Box_idat::read_data` only caught by `assert()`

**Branch:** `fix/idat-seek-assert` (off `master`)
**Severity:** Latent — observable when an upstream bound is bypassed.
**Files touched:** `libheif/box.cc`

## Summary

```cpp
bool success = istr->seek(m_data_start_pos + (std::streampos) start);
assert(success);
(void) success;
```

Both `seek()` and the subsequent `read()` are guarded only by
`assert()`. Under `-DNDEBUG` (the default in audit_repros, and the
typical release build) the assert is compiled out and execution
continues regardless of the seek result. If a bounds check above
ever fails to reject a bad position — e.g. through a regression in
the bug11/bug13 overflow handling — `seek()` silently returns false
and the subsequent `read()` populates `out_data` from whatever the
previous file position was. The data ends up looking valid to the
caller; this is the latent shape of the bug.

## Fix

Replace each assert with a hard error return:

```cpp
bool success = istr->seek(m_data_start_pos + (std::streampos) start);
if (!success) {
  return Error(heif_error_Invalid_input,
               heif_suberror_End_of_data,
               "Could not seek to idat data start");
}
...
success = istr->read(...);
if (!success) {
  return Error(heif_error_Invalid_input,
               heif_suberror_End_of_data,
               "Could not read idat data");
}
```

## Verification

`audit_repros/bug14_idat_seek_assert/` uses a `FailingSeekReader`
that always returns false from `seek()`. Before the fix the assert
is compiled out and the harness shows an ASAN heap-buffer-overflow
inside the still-attempted `istr->read()`. After:

```
[bug14] survived: err="Could not seek to idat data start" dest.size=0
```

The function returns the seek error; no read is attempted.

## PR title

`box: turn istr->seek/read failures in Box_idat::read_data into errors`

## PR body

```
seek() and read() in Box_idat::read_data were guarded only by
assert(), which is a no-op under NDEBUG. A regression in the
overflow handling above would let a bad position through and
silently leave out_data populated from whatever file position
remained — looks valid to the caller, leaks unrelated bytes.

Replace the asserts with hard error returns.
```
