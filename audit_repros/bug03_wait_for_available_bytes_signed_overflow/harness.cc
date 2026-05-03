// bug03 — int64_t target_size = uint64_t + size_t signed overflow.
// UBSAN-detectable.

#include <cstdio>
#include <cstdint>
#include <memory>
#include "bitstream.h"

int main() {
  uint8_t real_buf[64] = {0};
  uint64_t fake_length = static_cast<uint64_t>(INT64_MAX) + 1024; // > INT64_MAX
  auto r = std::make_shared<StreamReader_memory>(real_buf, (size_t)fake_length, false);

  // Seek to a position such that get_position() + nBytes > INT64_MAX.
  uint64_t pos = static_cast<uint64_t>(INT64_MAX) - 8;
  if (!r->seek(pos)) {
    std::fprintf(stderr, "[bug03] seek failed (size_t too narrow on this platform?)\n");
    return 0;
  }

  // BitstreamRange constructed over the full claimed range.
  BitstreamRange br(r, (size_t)fake_length);

  // wait_for_available_bytes(64) → int64_t target_size = (INT64_MAX - 8) + 64
  //                              → INT64_MAX + 56 → signed overflow UB.
  br.wait_for_available_bytes(64);

  std::fprintf(stderr, "[bug03] survived (UBSAN should have aborted on the addition)\n");
  return 0;
}
