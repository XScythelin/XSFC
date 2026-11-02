// Execute the production flashfs implementation against NOR-like host storage.
// Does not touch the connected board or its logs.
#include <vector>
#include <cassert>
#include <cstring>
#include "esp_partition.h"
#include "spi_flash_mmap.h"

esp_partition_t testPartition{3 * 1024 * 1024};
static std::vector<uint8_t> disk(testPartition.size, 0xff);
static bool flight = false, failWrite = false, corruptRead = false;
static unsigned writes = 0;
static size_t largestWrite = 0;
int esp_partition_write_raw(const esp_partition_t*, size_t offset, const void* bytes, size_t size)
{
  assert(!flight && offset + size <= disk.size());
  if(failWrite) return -1;
  ++writes;
  largestWrite = std::max(largestWrite, size);
  auto* src = static_cast<const uint8_t*>(bytes);
  // Journal must never become visible before all referenced data is durable.
  constexpr size_t journalBytes = 32 * 8;
  if(offset >= disk.size() - journalBytes)
  {
    uint32_t end = 0xffffffffu;
    if(size >= 8) std::memcpy(&end, src + 4, sizeof(end));
    if(end != 0xffffffffu)
      for(size_t i = 0; i < end; ++i) assert(disk[i] != 0xff);
  }
  for(size_t i = 0; i < size; ++i) disk[offset + i] &= src[i];
  return ESP_OK;
}
int esp_partition_read_raw(const esp_partition_t*, size_t offset, void* bytes, size_t size)
{
  assert(offset + size <= disk.size());
  std::memcpy(bytes, disk.data() + offset, size);
  if(corruptRead && size) static_cast<uint8_t*>(bytes)[0] ^= 1;
  return ESP_OK;
}
int esp_partition_erase_range(const esp_partition_t*, size_t offset, size_t size)
{
  assert(!flight && offset + size <= disk.size());
  std::memset(disk.data() + offset, 0xff, size);
  return ESP_OK;
}
int esp_partition_mmap(const esp_partition_t*, size_t offset, size_t, int, const void** out, esp_partition_mmap_handle_t*)
{ *out = disk.data() + offset; return ESP_OK; }
bool blackboxFlightActive() { return flight; }
bool blackboxUsePsram() { return true; }

#include "../../lib/Xsfc/src/Blackbox/BlackboxFlashfs.cpp"

static uint8_t pattern(size_t i) { return (i * 37 + i / 251) & 0x7f; }
static void finish()
{
  unsigned calls = 0;
  while(flashfsSavePending() && !stagingFailed)
  {
    const auto before = writes;
    flashfsCommitPending();
    assert(writes - before <= 1);
    assert(++calls < 10000);
  }
}
int main()
{
  assert(flashfsInit());
  constexpr size_t length = 2 * 1024 * 1024 + 19;
  flight = true;
  for(size_t i = 0; i < length; ++i) flashfsWriteByte(pattern(i));
  flashfsClose();
  assert(writes == 0 && flashfsSavePending());
  assert(!flashfsTakeSaveCompleted());
  flight = false;
  finish();
  assert(!flashfsSavePending() && flashfsTakeSaveCompleted());
  assert(!flashfsTakeSaveCompleted() && largestWrite <= 512);
  for(size_t i = 0; i < length; ++i) assert(disk[i] == pattern(i));
  // Reinitialize: journal and data must come from persisted storage.
  assert(flashfsInit() && flashfsGetOffset() == length);
  uint8_t tail[19];
  assert(flashfsReadAbs(length - 19, tail, 19) == 19);
  for(size_t i = 0; i < 19; ++i) assert(tail[i] == pattern(length - 19 + i));
  // A second log appends without touching the first.
  flight = true;
  for(unsigned i = 0; i < 777; ++i) flashfsWriteByte(0x35);
  flashfsClose();
  flight = false;
  finish();
  assert(flashfsTakeSaveCompleted());
  assert(flashfsInit() && flashfsGetOffset() == length + 777);
  for(size_t i = 0; i < length; ++i) assert(disk[i] == pattern(i));
  // Verification mismatch must retain interlock and suppress completion.
  flashfsWriteByte(0x12);
  flight = true;
  flashfsClose();
  flight = false;
  corruptRead = true;
  finish();
  assert(stagingFailed && flashfsSavePending() && !flashfsTakeSaveCompleted());
  corruptRead = false;
  // Reset ONLY the host's virtual disk, never the connected board.
  std::fill(disk.begin(), disk.end(), 0xff);
  assert(flashfsInit());
  flight = true;
  const size_t capacity = flashfsGetSize();
  for(size_t i = 0; i < capacity + 128; ++i) flashfsWriteByte(pattern(i));
  assert(flashfsIsEOF() && flashfsGetOffset() == capacity);
  flight = false;
  finish();
  assert(flashfsTakeSaveCompleted());
  assert(flashfsInit() && flashfsIsEOF() && flashfsGetOffset() == capacity);
  for(size_t i = 0; i < capacity; ++i) assert(disk[i] == pattern(i));
  std::fill(disk.begin(), disk.end(), 0xff);
  assert(flashfsInit());
  flight = true;
  flashfsWriteByte(0x23);
  flashfsClose();
  flight = false;
  failWrite = true;
  finish();
  assert(stagingFailed && flashfsSavePending() && !flashfsTakeSaveCompleted());
  failWrite = false;
  std::fill(disk.begin(), disk.end(), 0xff);
  assert(flashfsInit());
  for(unsigned log = 0; log < FLASHFS_JOURNAL_ITEMS; ++log)
  {
    flight = true;
    flashfsWriteByte(log);
    flashfsClose();
    flight = false;
    finish();
    assert(flashfsTakeSaveCompleted());
  }
  assert(flashfsIsEOF() && flashfsGetOffset() == FLASHFS_JOURNAL_ITEMS);
  const auto fullDisk = disk;
  flashfsWriteByte(0x55);
  flashfsClose();
  finish();
  assert(disk == fullDisk && !flashfsTakeSaveCompleted());
  assert(flashfsInit() && flashfsIsEOF());
  std::free(staging);
  staging = nullptr;
}
