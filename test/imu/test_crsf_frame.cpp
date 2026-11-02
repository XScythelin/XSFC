#include "Rc/Crsf.h"
#include <cassert>
#include <cstdint>
#include <cstring>

int main()
{
  using namespace Xsfc::Rc;
  struct GuardedFrame
  {
    CrsfMessage frame;
    uint8_t guard[8];
  } storage{};
  std::memset(storage.guard, 0xa5, sizeof(storage.guard));
  // Exercise the exact indexing used by InputCRSF::parse for every legal size.
  for(unsigned size = 2; size <= CRSF_FRAME_SIZE_MAX - 2; ++size)
  {
    auto* bytes = reinterpret_cast<uint8_t*>(&storage.frame);
    bytes[0] = CRSF_SYNC_BYTE;
    bytes[1] = size;
    bytes[2] = CRSF_FRAMETYPE_MSP_REQ;
    for(unsigned i = 3; i <= size; ++i) bytes[i] = i;
    bytes[size + 1] = storage.frame.crc();
    assert(bytes[size + 1] == storage.frame.crc());
    for(auto byte : storage.guard) assert(byte == 0xa5);
  }
}
