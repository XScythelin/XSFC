#include "Blackbox/BlackboxRate.hpp"
#include <cassert>
int main()
{
  using namespace Xsfc::Blackbox;
  assert(!loggingEnabled(0) && !loggingEnabled(2));
  assert(loggingEnabled(1) && loggingEnabled(3));
  for(unsigned i = 0; i <= 4; ++i) assert(sampleRateIndex(i, 250) == i);
  assert(sampleRateIndex(32, 250) == 2);
  assert(sampleRateIndex(32, 500) == 1);
  assert(sampleRateIndex(32, 0) == 0);
  assert(sampleRateIndex(65535, 1000) == 0);
}
