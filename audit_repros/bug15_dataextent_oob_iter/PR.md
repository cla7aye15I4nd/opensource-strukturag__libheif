# PR: bug15 — `DataExtent::read_data` OOB iterator advance

**Branch:** `fix/dataextent-oob-iter` (off `master`)
**Severity:** Latent — defense-in-depth.
**Files touched:** `libheif/codecs/decoder.cc`

## Summary

```cpp
if (!m_raw.empty()) {
  data.insert(data.begin(),
              m_raw.begin() + offset,
              m_raw.begin() + offset + size);
  return data;
}
```

When `m_raw` is pre-populated (caching path) and
`offset + size > m_raw.size()`, the iterator arithmetic advances
past `m_raw.end()` — UB and an OOB read.

Today `DataExtent` is freshly constructed before decode, so `m_raw`
starts empty and the bounds-checked `Source::Image` branch is taken
instead. The audit comment notes the latent risk if future code
reuses a `DataExtent` across calls, or if an iloc field-size
misalignment populates `m_raw` with fewer bytes than expected.

## Fix

Add a bounds check (with overflow guard) before the iterator
advance:

```cpp
if (size > std::numeric_limits<uint64_t>::max() - offset ||
    offset + size > m_raw.size()) {
  return Error{heif_error_Invalid_input,
               heif_suberror_End_of_data,
               "Read past end of cached DataExtent buffer"};
}
```

## Verification

`audit_repros/bug15_dataextent_oob_iter/` constructs a
`DataExtent` with 16 bytes in `m_raw` and asks for 64. Before the
fix: ASAN heap-buffer-overflow. After:

```
[bug15] survived (ASAN should have aborted)
```

The Result returns the new `Read past end of cached DataExtent
buffer` error.

## PR title

`decoder: bounds-check DataExtent::read_data cached-buffer path`

## PR body

```
DataExtent::read_data uses iterator arithmetic on the cached
buffer (m_raw) without checking offset + size <= m_raw.size().
Today the path is unreachable because m_raw starts empty; latent,
but the check is cheap and hardens against future reuse /
partial-fill regressions.

Add a bounds check (with overflow guard) before the iterator
advance.
```
