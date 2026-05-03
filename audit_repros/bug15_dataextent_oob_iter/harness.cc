// bug15 — DataExtent::read_data OOB iterator advance.

#include <cstdio>
#include <vector>

#include "codecs/decoder.h"

int main() {
  DataExtent ext;
  // Simulate the "cache pre-populated with fewer bytes than caller expects"
  // condition from the audit comment.
  ext.m_raw.assign(16, 0xAA);  // 16 bytes only.

  // Caller asks for 64 bytes starting at offset 0. m_raw.begin() + 0 + 64
  // advances 48 bytes past m_raw.end() — heap-buffer-overflow.
  auto res = ext.read_data(/*offset=*/0, /*size=*/64);
  // Whether `res` reports success/failure depends on libc++ vs libstdc++,
  // but ASAN catches the iterator advance regardless.
  std::fprintf(stderr, "[bug15] survived (ASAN should have aborted)\n");
  return 0;
}
