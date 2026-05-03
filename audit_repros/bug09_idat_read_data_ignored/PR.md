# PR: bug09 — propagate error from `idat->read_data()`

**Branch:** `fix/iloc-idat-error-propagation` (off `master`)
**Severity:** Confirmed bug.
**Files touched:** `libheif/box.cc`

## Summary

In `Box_iloc::read_data` the construction_method == 1 (idat) branch
called `idat->read_data(...)` without capturing its return value. If
the idat read failed — out-of-bounds offset, security-limit
exceeded, seek/read error — the error was silently dropped:

```cpp
idat->read_data(istr,
                extent.offset + item->base_offset,
                extent.length,
                *dest, limits);
// no err = ..., no `if (err) return err;`
```

The caller therefore received `Error::Ok` with a partially-populated
or empty `dest`, which downstream consumers treat as fully-read data
and run off the end of.

## Fix

Capture and propagate the error:

```cpp
Error idat_err = idat->read_data(istr,
                                 extent.offset + item->base_offset,
                                 extent.length,
                                 *dest, limits);
if (idat_err) {
  return idat_err;
}
```

## Verification

`audit_repros/bug09_idat_read_data_ignored/` — extent declares 4096
bytes, idat box contains 16. Before: harness reports
`err.code=0  bytes_read=16` (silent partial read). After:

```
[bug09] item 1: extent_total=4096  bytes_read=16  err.code=2 msg=""
```

The error is now surfaced, and the harness's downstream-consumer
ASAN trap is no longer reachable.

## PR title

`box: propagate error from idat->read_data() in Box_iloc::read_data`

## PR body

```
The return value of idat->read_data() was silently discarded. If
the idat read failed (out-of-bounds offset, security-limit
exceeded, seek/read error) the caller received Error::Ok with a
partially-populated dest — downstream consumers treat that as
fully-read data and OOB.

Capture and propagate the error.
```
