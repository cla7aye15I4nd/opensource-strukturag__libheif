// bug01 — StreamReader_memory::read end_pos = m_position + size overflow.
//
// Latent: in normal use, seek() rejects positions > m_length, so m_position
// stays bounded. We demonstrate the boundary by constructing a
// StreamReader_memory whose length field is near UINT64_MAX (a caller would
// only pass such a value if it lied about the buffer size — a precondition
// violation, but ASAN should still fire if that happens).

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <memory>

#include "bitstream.h"

int main() {
  // 64-byte real buffer.
  alignas(64) uint8_t real_buf[64] = {0xCA, 0xFE, 0xBA, 0xBE};

  // Construct the reader claiming a much larger size — this simulates a
  // caller that lies about its buffer length.
  uint64_t fake_length = static_cast<uint64_t>(-1) - 100; // UINT64_MAX - 100
  auto r = std::make_shared<StreamReader_memory>(real_buf, (size_t)fake_length, false);
  // Note: on a 64-bit system, size_t can hold UINT64_MAX-100, but on 32-bit
  // platforms this harness would need a different reproducer. libheif's
  // primary deployment is 64-bit, matching the AUDIT comment.

  // Seek near the top of the claimed range.
  bool ok = r->seek(fake_length - 16);
  std::fprintf(stderr, "[bug01] seek to high offset: %d\n", (int)ok);

  // Read a length that wraps end_pos = m_position + size.
  uint8_t out[200] = {0};
  bool got = r->read(out, 200);
  std::fprintf(stderr, "[bug01] read past wrap: %d (ASAN should fire here)\n", (int)got);
  // ^ With ASAN and the bug active this is a heap-buffer-overflow on real_buf.
  return 0;
}
