# bug08 — `extent.offset + base_offset + length` overflow analysis

**Source:** `libheif/box.cc:1857-1859`

**Severity:** Safe (proven). The audit comment is the proof:

> "extent.offset, item->base_offset, and extent.length are each individually
> checked against MAX_FILE_POS (2^55-1) above. Their sum is at most
> 3*(2^55-1) ≈ 1.08e17, which is well below UINT64_MAX (1.84e19). No
> overflow here."

This reproducer is a **regression watchdog**. It generates an iloc with
each component set to `MAX_FILE_POS` (2^55-1), the worst legal case, and
runs ASAN/UBSAN — confirming the addition does not overflow. If
`MAX_FILE_POS` is ever raised above 2^63/3 the next time someone touches
the limit, this reproducer fires under UBSAN.

## Reproduce

```
python3 gen_poc.py /tmp/bug08.heif
UBSAN_OPTIONS=halt_on_error=1 ./bug08 /tmp/bug08.heif
```

Expected output today: clean parse error (the values exceed the file size,
so `wait_for_file_size` returns `size_beyond_eof`). No UBSAN diagnostic.

## Real sanitizer output

Captured by running `./build.sh && ./run_all.sh`. Reproduced verbatim below (process IDs, hex addresses and BuildIds normalized for stable diffs):

```
[bug08] read_from_memory err=""
[bug08] append_data_from_iloc(id=1) err="Extent in iloc box references data outside of file bounds (points to file position 72057594037927934)
" (UBSAN must NOT have fired on extent.offset+base_offset+length)
```
