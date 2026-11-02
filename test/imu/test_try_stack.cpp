#include "Utils/TryStack.hpp"
#include <cassert>
#include <thread>
#include <atomic>

int main()
{
  Xsfc::Utils::TryStack<int, 8> stack;
  int value = 0;
  assert(!stack.pop(value));
  for(int i = 1; i <= 8; ++i) assert(stack.push(i));
  assert(stack.full());
  assert(!stack.push(9));
  for(int i = 8; i >= 1; --i) { assert(stack.pop(value)); assert(value == i); }
  assert(stack.empty());
  std::atomic<int> done{0};
  auto producer = [&] {
    for(int i = 1; i <= 200000; ++i) stack.push(i);
    ++done;
  };
  std::thread a(producer), b(producer);
  while(done < 2 || !stack.empty())
    if(stack.pop(value)) assert(value >= 1 && value <= 200000);
  a.join();
  b.join();
}
