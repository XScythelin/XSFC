#pragma once

#include <atomic>
#include <cstddef>

namespace Xsfc::Utils {

// Small multi-producer stack for best-effort notifications. Never wait for
// the other core: an overlapping notification can safely be dropped.
template<typename T, size_t Capacity>
class TryStack
{
public:
  bool push(const T& value)
  {
    if(_busy.test_and_set(std::memory_order_acquire)) return false;
    const bool room = _size < Capacity;
    if(room) _items[_size++] = value;
    _busy.clear(std::memory_order_release);
    return room;
  }

  bool pop(T& value)
  {
    if(_busy.test_and_set(std::memory_order_acquire)) return false;
    const bool available = _size > 0;
    if(available) value = _items[--_size];
    _busy.clear(std::memory_order_release);
    return available;
  }

  bool empty() const { return _size.load(std::memory_order_relaxed) == 0; }
  bool full() const { return _size.load(std::memory_order_relaxed) >= Capacity; }

private:
  std::atomic_flag _busy = ATOMIC_FLAG_INIT;
  std::atomic<size_t> _size{0};
  T _items[Capacity]{};
};

}
