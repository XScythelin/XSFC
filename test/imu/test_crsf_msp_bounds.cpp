#include "Rc/Crsf.h"
#include <cassert>
#include <cstring>

int main()
{
  using namespace Xsfc;
  Rc::CrsfMessage frame{};
  Connect::MspMessage message;
  uint8_t origin = 0;
  // Truncated headers must never underflow the available payload length.
  for(unsigned size = 0; size < 10; ++size)
  {
    for(unsigned version = 1; version <= 2; ++version)
    {
      message = Connect::MspMessage();
      frame.size = size;
      std::memset(frame.payload, 0xff, sizeof(frame.payload));
      frame.payload[2] = 0x10 | (version << 5);
      assert(!Rc::Crsf::decodeMsp(frame, message, origin));
      assert(message.received == 0);
    }
  }
  // Declared MSP payload larger than local storage is rejected.
  frame.size = 10;
  frame.payload[2] = 0x50;
  frame.payload[3] = 0;
  frame.payload[4] = 102;
  frame.payload[5] = 0;
  frame.payload[6] = 0xff;
  frame.payload[7] = 0xff;
  assert(!Rc::Crsf::decodeMsp(frame, message, origin));
  // A valid zero-payload V1 RAW_IMU request still decodes.
  frame.size = 7;
  frame.payload[2] = 0x30;
  frame.payload[3] = 0;
  frame.payload[4] = 102;
  assert(Rc::Crsf::decodeMsp(frame, message, origin));
  assert(message.cmd == 102 && message.isCmd());
  // Last-line append guard: do not partially copy an oversized fragment.
  uint8_t bytes[Connect::MSP_BUF_SIZE + 1]{};
  message = Connect::MspMessage();
  assert(message.append(bytes, sizeof(bytes)) == 0);
  assert(!message.isReady());
  assert(message.append(bytes, Connect::MSP_BUF_SIZE) == Connect::MSP_BUF_SIZE);
  assert(message.append(bytes, 1) == 0);
}
