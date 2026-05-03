# PR: bug12 — `start + length > get_box_size()` is provably safe

**Branch:** none — no code change required.
**Severity:** Safe (proven).

## Summary

The audit comment at this site analyses the `start + length`
addition in `Box_idat::read_data` and proves it cannot overflow:

> `start` and `length` are each bounded by `MAX_FILE_POS = 2^55-1`
> above, so their sum is at most `2 * (2^55-1)` — well within
> `UINT64_MAX`.

`master` does not contain the AUDIT comment block, so there is
nothing to remove either. No PR is required.

The watchdog reproducer in
`audit_repros/bug12_idat_start_plus_length_safe/` is kept in tree as
a regression alarm.

## Verification

```
[bug12] read_data err="idat box contained 36028797018963967 bytes,
total memory size would be 36028797018963967 bytes, exceeding the
security limit of 4294967296 bytes" (UBSAN must not fire)
```

UBSAN-clean.
