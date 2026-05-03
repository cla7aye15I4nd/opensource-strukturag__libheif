# PR: bug03 — `wait_for_available_bytes` signed/unsigned mix

**Branch:** `fix/wait-for-available-bytes-overflow` (off `master`)
**Severity:** Low — wrong error, not memory unsafety.
**Files touched:** `libheif/bitstream.cc`

## Summary

```cpp
int64_t target_size = m_istr->get_position() + nBytes;
return m_istr->wait_for_file_size(target_size);
```

`get_position()` returns `uint64_t`, `nBytes` is `size_t`, but the sum
is stored in `int64_t`. When the unsigned sum exceeds `INT64_MAX` the
implicit conversion is undefined; the resulting negative value is then
implicitly widened back to a huge `uint64_t` for
`wait_for_file_size()`, which returns `size_beyond_eof` and produces
a parse error instead of a real bounds answer.

UBSAN with `-fsanitize=integer` catches it as a signed-integer
implicit-conversion error.

## Fix

Use `uint64_t` throughout and guard the addition against wrap:

```cpp
uint64_t pos = m_istr->get_position();
if (nBytes > std::numeric_limits<uint64_t>::max() - pos) {
  return StreamReader::grow_status::size_beyond_eof;
}
uint64_t target_size = pos + nBytes;
return m_istr->wait_for_file_size(target_size);
```

## Verification

`audit_repros/bug03_wait_for_available_bytes_signed_overflow/`. Before
the fix UBSAN aborts with
`runtime error: implicit conversion ... changed the value to ...`.
After:

```
[bug03] survived (UBSAN should have aborted on the addition)
```

## PR title

`bitstream: avoid signed-conversion UB in wait_for_available_bytes`

## PR body

```
BitstreamRange::wait_for_available_bytes() computed
`int64_t target_size = get_position() + nBytes`. When the unsigned
sum exceeds INT64_MAX, the implicit conversion to int64_t is
undefined behaviour; the resulting negative value widens back to a
huge uint64_t when handed to wait_for_file_size(), masking the real
bound check with a spurious size_beyond_eof.

Use uint64_t for the target and add an overflow guard.
```
