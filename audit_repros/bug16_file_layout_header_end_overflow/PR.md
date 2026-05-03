# PR: bug16 — `next_box_start + MAXIMUM_BOX_HEADER_SIZE` overflow

**Branch:** `fix/file-layout-header-end-overflow` (off `master`)
**Severity:** Low — defence-in-depth.
**Files touched:** `libheif/file_layout.cc`

## Summary

```cpp
uint64_t next_box_header_end = next_box_start + MAXIMUM_BOX_HEADER_SIZE;
```

Through the normal box traversal, `next_box_start` is bounded by
`m_max_length` (each iteration advances by `get_box_size()` which is
itself checked against `m_max_length`), so today the addition of
`MAXIMUM_BOX_HEADER_SIZE = 32` cannot wrap. The audit comment notes
the "combined overflow of next_box_start … PLUS this addition is not
formally proved safe."

## Fix

Add an explicit saturation guard:

```cpp
if (next_box_start > std::numeric_limits<uint64_t>::max() - MAXIMUM_BOX_HEADER_SIZE) {
  return {heif_error_Invalid_input,
          heif_suberror_Unspecified,
          "Box position too large, integer overflow"};
}
```

This makes the loop formally safe even if a future change (e.g. a
64-bit length API or a different stream reader contract) breaks the
upstream invariant.

## Verification

`audit_repros/bug16_file_layout_header_end_overflow/` (a watchdog
that drives the boundary directly):

```
read_from_reader err: code=2 sub=100 msg=Invalid input: Unexpected end of file
```

The boundary is reached and the safe path is taken — UBSAN-clean.

## PR title

`file_layout: saturation guard before next_box_start + MAXIMUM_BOX_HEADER_SIZE`

## PR body

```
Through normal traversal next_box_start is bounded by m_max_length
and the +32 addition cannot wrap. Add an explicit saturation check
as defence-in-depth so the loop stays formally safe if a future
change (64-bit length API, different reader contract) breaks the
upstream invariant.
```
