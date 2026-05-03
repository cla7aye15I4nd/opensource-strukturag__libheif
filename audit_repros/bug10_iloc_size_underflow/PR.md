# PR: bug10 — `size -= extent.length` integer underflow on idat extents

**Branch:** `fix/iloc-size-underflow` (off `master`)
**Severity:** **High**.
**Files touched:** `libheif/box.cc`

## Summary

In `Box_iloc::read_data` the construction_method == 1 (idat) branch
ignored the caller's `size` cap, always read `extent.length` bytes,
and then unconditionally did:

```cpp
idat->read_data(istr, ..., extent.length, ...);
size -= extent.length;     // unsigned wrap if extent.length > size
```

When `extent.length > size`, `size` wraps to `~0ULL`. On a subsequent
extent the `min(extent.length, size)` clamp in the
construction_method == 0 branch then evaluates to `extent.length`,
letting the parser copy far more bytes into `dest` than the caller
asked for.

## Fix

Mirror the construction_method == 0 clamp:

```cpp
uint64_t idat_read_len = std::min(extent.length, size);
idat->read_data(istr, ..., idat_read_len, ...);
size -= idat_read_len;
```

`idat_read_len <= size` ⇒ subtraction cannot wrap.

This change only addresses the underflow / over-read. The separate
issue that `idat->read_data()`'s return value is still dropped is
the `fix/iloc-idat-error-propagation` PR.

## Verification

`audit_repros/bug10_iloc_size_underflow/` — iloc with two extents
(idat 4096 bytes + file 16 bytes), caller passes `size = 4`.

| Run    | bytes_read |
|--------|-----------:|
| Before | 4096 (over-read) |
| After  | 4 (clamped)     |

Post-fix log:

```
[bug10] append_data_from_iloc(id=1, size=4): err=""  bytes_read=4
[bug10] append_data_from_iloc(id=2, size=4): err=""  bytes_read=4
```

## PR title

`box: clamp iloc idat extent read length and saturate size accounting`

## PR body

```
On the construction_method == 1 (idat) branch of
Box_iloc::read_data, `size -= extent.length` wraps when the extent
declares more bytes than the caller requested. On the next extent
the construction_method == 0 clamp then evaluates against ~0ULL
and copies far more than budgeted.

Compute idat_read_len = min(extent.length, size), pass that to
idat->read_data(), then subtract idat_read_len from size — mirrors
the existing clamp on the file-extent branch.

The dropped error from idat->read_data() is addressed in the
fix/iloc-idat-error-propagation PR.
```
