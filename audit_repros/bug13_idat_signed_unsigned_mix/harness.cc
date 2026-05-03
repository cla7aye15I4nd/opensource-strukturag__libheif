// bug13 — `(int64_t)m_data_start_pos + start + length` mixed signed/unsigned.
//
// The audit comment claims signed-overflow UB. Empirically the addition is
// performed as `int64_t + uint64_t = uint64_t` so signed-integer-overflow
// UBSAN does NOT fire. What DOES happen is that the resulting uint64_t
// passes through `wait_for_file_size` (also uint64_t), and the next line
// (the seek) advances the cursor to a wrapped position. With an unbounded
// reader this exposes an ASAN heap-buffer-overflow on the subsequent read.
//
// Setup mirrors bug11: forge an idat with largesize = MAX_LARGE_BOX_SIZE,
// LyingPositionReader with huge get_position() and unbounded read.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "bitstream.h"
#include "box.h"
#include "security_limits.h"

class LyingPositionReader : public StreamReader {
public:
  LyingPositionReader(const uint8_t* data, size_t real_size, uint64_t lie_offset)
    : m_data(data), m_real_size(real_size), m_lie(lie_offset) {}
  uint64_t get_position() const override { return m_lie + m_pos; }
  StreamReader::grow_status wait_for_file_size(uint64_t) override {
    return StreamReader::grow_status::size_reached;
  }
  bool read(void* dst, size_t n) override {
    std::memcpy(dst, m_data + m_pos, n); m_pos += n; return true;
  }
  bool seek(uint64_t pos) override {
    if (pos < m_lie) return false;
    m_pos = pos - m_lie;
    return true;
  }
private:
  const uint8_t* m_data; size_t m_real_size; uint64_t m_lie; uint64_t m_pos = 0;
};

static std::vector<uint8_t> forge_idat() {
  std::vector<uint8_t> b;
  for (int i = 24; i >= 0; i -= 8) b.push_back((uint8_t)((1u >> i) & 0xFF));
  const char* t = "idat";
  for (int i = 0; i < 4; i++) b.push_back((uint8_t)t[i]);
  uint64_t largesize = (uint64_t)MAX_LARGE_BOX_SIZE;
  for (int i = 56; i >= 0; i -= 8) b.push_back((uint8_t)((largesize >> i) & 0xFF));
  for (int i = 0; i < 16; i++) b.push_back((uint8_t)i);
  return b;
}

int main() {
  auto buf = forge_idat();
  auto reader = std::make_shared<LyingPositionReader>(buf.data(), buf.size(),
                                                      0xFFFFFFFFFFFFFF00ULL);
  BitstreamRange range(reader, /*length=*/(size_t)INT64_MAX);

  std::shared_ptr<Box> generic;
  heif_security_limits limits = *heif_get_global_security_limits();
  Box::read(range, &generic, &limits);
  auto idat = std::dynamic_pointer_cast<Box_idat>(generic);
  if (!idat) { std::fprintf(stderr, "[bug13] not idat\n"); return 0; }

  std::vector<uint8_t> dest;
  // Drive `(int64_t)m_data_start_pos + start + length` past the buggy
  // expression and on to the seek + read OOB.
  Error e = idat->read_data(reader, /*start=*/1, /*length=*/64, dest, &limits);
  std::fprintf(stderr, "[bug13] survived: err=\"%s\"\n", e.message.c_str());
  return 0;
}
