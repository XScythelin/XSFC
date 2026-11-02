#pragma once
#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <cstring>
#define IRAM_ATTR
#define FLASHFS_WRITE_BUFFER_SIZE 128u
#define FLASHFS_FLUSH_BUFFER_SIZE 64u
#define FLASHFS_JOURNAL_ITEMS 32u
#define SPI_FLASH_SEC_SIZE 4096u
struct FlashfsJournalItem { uint32_t logBegin, logEnd; };
struct FlashfsRuntime {
  const void* partition;
  void* buffer;
  uint32_t address;
  FlashfsJournalItem journal[FLASHFS_JOURNAL_ITEMS];
  uint32_t journalIdx;
};
#define FLASHFS_JOURNAL_SIZE (FLASHFS_JOURNAL_ITEMS * sizeof(FlashfsJournalItem))
void flashfsJournalLoad(FlashfsJournalItem*, size_t, size_t);
void flashfsJournalFix(FlashfsJournalItem*, size_t);
int flashfsInit();
int flashfsReadAbs(uint32_t, uint8_t*, unsigned);
void flashfsWriteAbs(uint32_t, const uint8_t*, unsigned);
bool flashfsIsEOF();
uint32_t flashfsGetSize();
void flashfsClose();
