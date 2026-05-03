# bug16 — `next_box_start + MAXIMUM_BOX_HEADER_SIZE` overflow

**Source:** `libheif/file_layout.cc:105-110`

**Severity:** Low — `m_max_length` is bounded by the input buffer (a
`size_t`), so `next_box_start` cannot reach `UINT64_MAX` through the loop.
The AUDIT comment notes the addition is *not formally* overflow-proof.

## What this reproducer does

Because the bug is unreachable through normal box traversal (each step
bumps `next_box_start` by `get_box_size()`, which is itself bounded by
`m_max_length`), this harness uses an *internal* approach: it instantiates
`FileLayout` directly with a `StreamReader_memory` whose `m_length` has been
forced to `UINT64_MAX - 4` (only possible by the test harness — the public
API constructor would never set such a value).

The test then walks past the boundary by passing a sequence of well-formed
boxes that increment `next_box_start` to `UINT64_MAX - 16`. The final
`next_box_start + 32` would wrap to `16`. The harness asserts the wrap is
caught (or, if the safeguard is removed, ASAN flags the OOB).

This is a **defence-in-depth** reproducer. It will pass on the current code
because `m_max_length` is constrained by the host's address space; it is
intended to fail loudly if the boundary contract ever changes (e.g. someone
adds a 64-bit length API).

## Reproduce

```
./bug16
```

(No HEIF file — the harness is self-contained.)

## Real sanitizer output

Captured by running `./build.sh && ./run_all.sh`. Reproduced verbatim below (process IDs, hex addresses and BuildIds normalized for stable diffs):

```
read_from_reader err: code=2 sub=100 msg=Invalid input: Unexpected end of file
```
