// bug12 — start + length > get_box_size() (proven safe — UBSAN watchdog).
#include <cstdio>
#include <cstdint>
#include <vector>
#include <memory>

#include "bitstream.h"
#include "box.h"

static std::vector<uint8_t> normal_idat_stream() {
  std::vector<uint8_t> b;
  // size32 = 16 (header + payload), type = "idat", payload = 8 zero bytes.
  for (int i = 24; i >= 0; i -= 8) b.push_back((uint8_t)((16u >> i) & 0xFF));
  const char* t = "idat";
  for (int i = 0; i < 4; i++) b.push_back((uint8_t)t[i]);
  for (int i = 0; i < 8; i++) b.push_back(0);
  return b;
}

int main() {
  auto buf = normal_idat_stream();
  auto reader = std::make_shared<StreamReader_memory>(buf.data(), buf.size(), false);
  BitstreamRange range(reader, buf.size());

  std::shared_ptr<Box> generic;
  heif_security_limits limits = *heif_get_global_security_limits();
  Error err = Box::read(range, &generic, &limits);
  auto idat = std::dynamic_pointer_cast<Box_idat>(generic);
  if (!idat) { std::fprintf(stderr, "[bug12] no idat\n"); return 0; }

  // The audit names MAX_FILE_POS as 2^55-1 — call read_data with that
  // upper-bound `start` and `length`.
  uint64_t MAX_FILE_POS = (UINT64_C(1) << 55) - 1;
  std::vector<uint8_t> dest;
  Error e = idat->read_data(reader, MAX_FILE_POS, MAX_FILE_POS, dest, &limits);
  std::fprintf(stderr, "[bug12] read_data err=\"%s\" (UBSAN must not fire)\n",
               e.message.c_str());
  return 0;
}
