// bug02 — BitstreamRange ctor underflow → OOB heap read.
//
// Triggers ASAN heap-buffer-overflow by constructing the exact buggy state:
// a StreamReader_memory whose `m_length` claims a large buffer but whose
// underlying allocation is only 64 bytes, plus a BitstreamRange built with
// `end < start`. With NDEBUG the assert is gone; m_remaining underflows to
// ~SIZE_MAX; subsequent reads pass the StreamReader_memory bounds check
// (m_length lies) and memcpy from past the real allocation.
//
// This setup mirrors what would happen when bug17 or bug18 fed the
// constructor with `end = (small wrap)` while `start = (small valid)`.
// Today the public API path is blocked by the MAX_LARGE_BOX_SIZE check at
// box.cc:261; this direct harness exercises the consequence of that check
// being bypassed (e.g. by a future regression or a stream reader whose
// `largesize` plumbing differs).

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "bitstream.h"

int main() {
  // 64-byte real buffer on the heap (so ASAN tracks it).
  std::vector<uint8_t> real_buf(64, 0xAB);
  uint64_t fake_length = static_cast<uint64_t>(1) << 30;  // 1 GiB lie

  auto reader = std::make_shared<StreamReader_memory>(real_buf.data(),
                                                      (size_t)fake_length, false);

  // Reach the buggy state: end < start. Today the call sites that produce
  // this are gated by the MAX_LARGE_BOX_SIZE check, so this is the
  // "post-mitigation-bypass" demonstration.
  BitstreamRange r(reader, /*start=*/100, /*end=*/4);
  // m_remaining = 4 - 100 = ~SIZE_MAX.

  // Walk past the 64-byte real buffer. Each read8() flows through
  // StreamReader_memory::read which does memcpy from m_data[m_position] —
  // the lying m_length lets the bounds check pass, and the memcpy reads
  // past real_buf.end() → ASAN heap-buffer-overflow.
  uint8_t sink = 0;
  for (int i = 0; i < 256; ++i) {
    sink ^= r.read8();
  }
  std::fprintf(stderr, "[bug02] survived OOB walk, sink=0x%02x\n", sink);
  return 0;
}
