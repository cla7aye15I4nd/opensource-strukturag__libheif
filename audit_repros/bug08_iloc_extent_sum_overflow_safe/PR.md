# PR: bug08 — `extent.offset + base_offset + length` is provably safe

**Branch:** none — no code change required.
**Severity:** Safe (proven). The audit comment is the proof, not a
bug.

## Summary

The audit comment at this site analyses
`extent.offset + item->base_offset + extent.length` and proves it
cannot overflow:

> Each operand is individually checked against `MAX_FILE_POS = 2^55-1`
> above. Their sum is at most `3 * (2^55-1) ≈ 1.08e17`, well below
> `UINT64_MAX (≈ 1.84e19)`.

`master` does not contain the AUDIT comment block, so there is nothing
to remove either. No PR is required.

The watchdog reproducer in
`audit_repros/bug08_iloc_extent_sum_overflow_safe/` exercises the
worst legal sum and confirms UBSAN does not fire — kept in tree as a
regression alarm.

## Verification

```
[bug08] append_data_from_iloc(id=1) err="Extent in iloc box references
data outside of file bounds (points to file position 72057594037927934)"
(UBSAN must NOT have fired on extent.offset+base_offset+length)
```

UBSAN-clean.

## Note

If `MAX_FILE_POS` is ever raised above `UINT64_MAX / 3`, the watchdog
will fire under UBSAN and the addition would have to be reworked.
