#include <Arduino.h>
#include <platform.h>

#ifdef USE_FLASHFS

#if defined(ESP32P4)
#include <spi_flash_mmap.h>
#include <esp_heap_caps.h>
#include <atomic>
#endif

#include "Device/FlashDevice.h"
#include "Utils/RingBuf.h"
#include "Utils/Math.hpp"
#if defined(ESP32P4)
bool blackboxFlightActive();
bool blackboxUsePsram();
void flashfsCommitPending();
#endif

static constexpr uint32_t FLASHFS_ERASED_VAL = 0xffffffff;

using BufferType = Xsfc::Utils::RingBuf<uint8_t, FLASHFS_WRITE_BUFFER_SIZE>;

static BufferType buff;

static FlashfsRuntime flashfs;
#if defined(ESP32P4)
// Stage the recording in PSRAM. Internal-flash programming stalls execution
// and must not run in the flight control path. Commit only on log close.
static uint8_t* staging = nullptr;
static const uint8_t* savedImage = nullptr;
static esp_partition_mmap_handle_t savedMapping;
static bool useStaging = true;
static bool modeInitialized = false;
static uint32_t dataBegin = UINT32_MAX, dataEnd = 0;
static uint32_t journalBegin = UINT32_MAX, journalEnd = 0;
static bool stagingFailed = false;
static uint32_t commitData = 0, commitJournal = 0;
static bool commitStarted = false;
static std::atomic<bool> commitPending{false};
static std::atomic<bool> saveCompleted{false};

bool flashfsTakeSaveCompleted()
{
    return saveCompleted.exchange(false, std::memory_order_acq_rel);
}

bool flashfsSavePending()
{
    return commitPending.load(std::memory_order_acquire);
}

static void commitStaging()
{
    const auto p = reinterpret_cast<const esp_partition_t*>(flashfs.partition);
    if(!p || !staging) return;
    uint8_t chunk[512];
    const auto writeRange = [&](uint32_t begin, uint32_t end) {
        while(begin < end)
        {
            const size_t count = std::min(sizeof(chunk), size_t(end - begin));
            memcpy(chunk, staging + begin, count);
            if(esp_partition_write_raw(p, begin, chunk, count) != ESP_OK) return false;
            begin += count;
        }
        return true;
    };
    if(!writeRange(dataBegin, dataEnd) || !writeRange(journalBegin, journalEnd))
    { stagingFailed = true; return; }
    dataBegin = journalBegin = UINT32_MAX;
    dataEnd = journalEnd = 0;
}

void flashfsCommitPending()
{
    if(!commitPending || blackboxFlightActive()) return;
    if(stagingFailed) return;
    const auto p = reinterpret_cast<const esp_partition_t*>(flashfs.partition);
    if(!p || !staging) { stagingFailed = true; return; }
    if(!commitStarted)
    {
        commitData = dataBegin;
        commitJournal = journalBegin;
        commitStarted = true;
    }
    // One bounded write per service call. A whole multi-second recording
    // must not monopolize the control task after disarm or starve watchdogs.
    uint32_t* cursor = commitData < dataEnd ? &commitData : &commitJournal;
    const uint32_t end = commitData < dataEnd ? dataEnd : journalEnd;
    if(*cursor < end)
    {
        uint8_t chunk[512];
        const size_t count = std::min(sizeof(chunk), size_t(end - *cursor));
        memcpy(chunk, staging + *cursor, count);
        if(esp_partition_write_raw(p, *cursor, chunk, count) != ESP_OK)
        { stagingFailed = true; return; }
        // Do not announce success for an unverified flash write. Read into
        // internal RAM, not the cached mmap or the PSRAM staging overlay.
        uint8_t verified[sizeof(chunk)];
        if(esp_partition_read_raw(p, *cursor, verified, count) != ESP_OK ||
           memcmp(chunk, verified, count) != 0)
        { stagingFailed = true; return; }
        *cursor += count;
        return;
    }
    if(commitData >= dataEnd && commitJournal >= journalEnd)
    {
        dataBegin = journalBegin = UINT32_MAX;
        dataEnd = journalEnd = 0;
        commitStarted = false;
        saveCompleted.store(true, std::memory_order_release);
        commitPending = false;
    }
}
#endif

const FlashfsRuntime * flashfsGetRuntime()
{
    return &flashfs;
}

static uint32_t IRAM_ATTR flashfsJournalAddress(size_t index, bool getEnd = false)
{
    uint32_t journalBase = reinterpret_cast<const esp_partition_t*>(flashfs.partition)->size - FLASHFS_JOURNAL_SIZE;
    return journalBase + (index * sizeof(FlashfsJournalItem)) + (getEnd * sizeof(uint32_t));
}

void flashfsJournalLoad(FlashfsJournalItem * data, size_t start, size_t num)
{
    size_t size = num * sizeof(FlashfsJournalItem);
    flashfsReadAbs(flashfsJournalAddress(start), (uint8_t*)data, size);
}

static void IRAM_ATTR flashfsLogBegin()
{
    uint32_t beginAddr = flashfs.address;
    size_t idx = flashfs.journalIdx;

    flashfs.journal[idx].logBegin = beginAddr;

    if(!flashfs.partition) return;

    size_t address = flashfsJournalAddress(idx);

    flashfsWriteAbs(address, (uint8_t*)&beginAddr, sizeof(uint32_t));
}

static bool IRAM_ATTR flashfsLogStarted()
{
    return flashfs.journalIdx < FLASHFS_JOURNAL_ITEMS && flashfs.journal[flashfs.journalIdx].logBegin != FLASHFS_ERASED_VAL;
}

static void IRAM_ATTR flashfsLogEnd()
{
    uint32_t endAddr = flashfs.address;
    size_t idx = flashfs.journalIdx;

    flashfs.journal[idx].logEnd = endAddr;

    flashfs.journalIdx++;

    if(!flashfs.partition) return;

    // idx is the log being closed, not the next journal slot.
    size_t address = flashfsJournalAddress(idx, true);

    flashfsWriteAbs(address, (uint8_t*)&endAddr, sizeof(uint32_t));
}

void flashfsJournalFix(FlashfsJournalItem * data, size_t num)
{
    uint8_t buff[4];
    for(size_t i = 0; i < num; i++)
    {
        if(data[i].logBegin != FLASHFS_ERASED_VAL && data[i].logEnd == FLASHFS_ERASED_VAL)
        {
            uint32_t addr = data[i].logBegin;
            uint32_t end = flashfsGetSize();
            while(addr < end)
            {
                flashfsReadAbs(addr, buff, 4);
                if(*reinterpret_cast<uint32_t*>(&buff[0]) == FLASHFS_ERASED_VAL)
                {
                    flashfs.address = addr;
                    flashfsLogEnd();
                    break;
                }
                addr += 128;
            }
            break;
        }
    }
}

int flashfsInit(void)
{
    flashfs.partition = Xsfc::Device::FlashDevice::findPartition();
    if(!flashfs.partition) return 0;
#if defined(ESP32P4)
    const auto partition = reinterpret_cast<const esp_partition_t*>(flashfs.partition);
    if(!modeInitialized)
    {
        useStaging = blackboxUsePsram();
        modeInitialized = true;
    }
    if(useStaging)
    {
        if(!staging) staging = static_cast<uint8_t*>(heap_caps_malloc(partition->size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if(!staging) { flashfs.partition = nullptr; return 0; }
        // Read through the cache instead of entering the flash driver's
        // cross-core cache-disable protocol for every small startup read.
        if(!savedImage)
        {
            const void* mapped = nullptr;
            if(esp_partition_mmap(partition, 0, partition->size,
                                  ESP_PARTITION_MMAP_DATA, &mapped, &savedMapping) != ESP_OK)
            { flashfs.partition = nullptr; return 0; }
            savedImage = static_cast<const uint8_t*>(mapped);
        }
        // Old logs stay in flash; only newly written ranges live in PSRAM.
        // Do not copy the entire partition during startup.
    }
    stagingFailed = false;
    commitPending = false;
    saveCompleted = false;
    commitStarted = false;
    dataBegin = journalBegin = UINT32_MAX;
    dataEnd = journalEnd = 0;
#endif

    flashfs.buffer = (void*)&buff;
    buff = BufferType();
    flashfs.journalIdx = 0;
    flashfs.address = 0;

    flashfsJournalLoad(flashfs.journal, 0, FLASHFS_JOURNAL_ITEMS);
    for(size_t i = 0; i < FLASHFS_JOURNAL_ITEMS; i++)
    {
        const auto& it = flashfs.journal[i];
        if(it.logEnd == FLASHFS_ERASED_VAL) break;
        flashfs.address = it.logEnd;
        flashfs.journalIdx++;
    }
    flashfsJournalFix(flashfs.journal, FLASHFS_JOURNAL_ITEMS);
#if defined(ESP32P4)
    commitStaging();
#endif

    return 1;
}

bool IRAM_ATTR flashfsFlushAsync(bool force)
{
    (void)force;
    if(flashfsIsEOF()) return false;
    if(!flashfsLogStarted()) flashfsLogBegin();

    auto buffer = reinterpret_cast<BufferType*>(flashfs.buffer);
    const size_t size = buffer->size();
    if(flashfs.partition && size > 0)
    {
        //uint32_t newAddress = force ? (flashfs.address + size) : Xsfc::Utils::alignAddressToWrite(flashfs.address, size, FLASHFS_FLUSH_BUFFER_SIZE);
        //size_t toWrite = newAddress - flashfs.address;
        uint8_t tmp[FLASHFS_FLUSH_BUFFER_SIZE];
        size_t chunks = (size / FLASHFS_FLUSH_BUFFER_SIZE) + 1;
        while(chunks--)
        {
            size_t len = buffer->pop(tmp, FLASHFS_FLUSH_BUFFER_SIZE);
            flashfsWriteAbs(flashfs.address, tmp, len);
            flashfs.address += len;
        }
        return true;
    }
    return false;
}

void IRAM_ATTR flashfsWriteByte(uint8_t byte)
{
    if(flashfsIsEOF()) return;

    auto buffer = reinterpret_cast<BufferType*>(flashfs.buffer);
    buffer->push(byte);
    const size_t size = buffer->size();

    if(flashfs.address + size >= flashfsGetSize())
    {
        flashfsClose();
        return;
    }

    if((flashfs.address + size) % SPI_FLASH_SEC_SIZE == 0)
    {
        flashfsFlushAsync(true);
        return;
    }

    if(size >= FLASHFS_FLUSH_BUFFER_SIZE)
    {
        flashfsFlushAsync(false);
    }
}

void IRAM_ATTR flashfsWrite(const uint8_t *data, unsigned int len, bool sync)
{
    (void)sync;
    while(len)
    {
        flashfsWriteByte(*data);
        data++;
        len--;
    }
}

void IRAM_ATTR flashfsWriteAbs(uint32_t address, const uint8_t *data, unsigned int len)
{
    if(!flashfs.partition) return;
    const auto p = reinterpret_cast<const esp_partition_t*>(flashfs.partition);
    if(!len) return;
#if defined(ESP32P4)
    if(!useStaging)
    {
        if(address > p->size || len > p->size - address ||
           esp_partition_write_raw(p, address, data, len) != ESP_OK) stagingFailed = true;
        return;
    }
    if(!staging || address > p->size || len > p->size - address) { stagingFailed = true; return; }
    memcpy(staging + address, data, len);
    if(address >= p->size - FLASHFS_JOURNAL_SIZE)
    { journalBegin = std::min(journalBegin, address); journalEnd = std::max(journalEnd, address + len); }
    else
    { dataBegin = std::min(dataBegin, address); dataEnd = std::max(dataEnd, address + len); }
#else
    esp_partition_write_raw(p, address, data, len);
#endif
}

int IRAM_ATTR flashfsReadAbs(uint32_t address, uint8_t *data, unsigned int len)
{
    if(!flashfs.partition) return 0;

    const auto p = reinterpret_cast<const esp_partition_t*>(flashfs.partition);
    if(address >= p->size) return 0;
    len = std::min((uint32_t)len, p->size - address);
#if defined(ESP32P4)
    if(useStaging && staging)
    {
        memcpy(data, savedImage + address, len);
        const auto overlay = [&](uint32_t begin, uint32_t end) {
            const uint32_t first = std::max(address, begin);
            const uint32_t last = std::min(address + len, end);
            if(first < last) memcpy(data + first - address, staging + first, last - first);
        };
        overlay(dataBegin, dataEnd);
        overlay(journalBegin, journalEnd);
        return len;
    }
#endif
    if(esp_partition_read_raw(p, address, data, len) == ESP_OK)
    {
        return len;
    }
    return 0;
}

bool IRAM_ATTR flashfsIsSupported(void)
{
    return (bool)flashfs.partition;
}

bool IRAM_ATTR flashfsIsReady(void)
{
    return flashfsIsSupported();
}

bool IRAM_ATTR flashfsIsEOF(void)
{
#if defined(ESP32P4)
    if(stagingFailed) return true;
#endif
    return !flashfs.partition || flashfs.journalIdx >= FLASHFS_JOURNAL_ITEMS || flashfs.address >= flashfsGetSize();
}

void flashfsEraseCompletely(void)
{
#if defined(ESP32P4)
    if(blackboxFlightActive()) return;
#endif
    const auto p = reinterpret_cast<const esp_partition_t*>(flashfs.partition);
    if(!p) return;
    esp_partition_erase_range(p, 0, p->size);
    flashfsInit();
}

void IRAM_ATTR flashfsClose(void)
{
    if(!flashfs.partition || flashfs.journalIdx >= FLASHFS_JOURNAL_ITEMS) return;
    if(!flashfsLogStarted() && buff.size() == 0) return;
    flashfsFlushAsync(true);
    if(!flashfsLogStarted()) return;
    flashfsLogEnd();
#if defined(ESP32P4)
    if(useStaging)
    {
        commitPending = true;
        flashfsCommitPending();
    }
#endif
}

uint32_t IRAM_ATTR flashfsGetWriteBufferFreeSpace(void)
{
    auto buffer = reinterpret_cast<BufferType*>(flashfs.buffer);
    return buffer->available();
}

uint32_t IRAM_ATTR flashfsGetWriteBufferSize(void)
{
    return FLASHFS_WRITE_BUFFER_SIZE;
}

uint32_t IRAM_ATTR flashfsGetSize(void)
{
    const auto p = reinterpret_cast<const esp_partition_t*>(flashfs.partition);
    return p ? p->size - FLASHFS_JOURNAL_SIZE : 0;
}

uint32_t flashfsGetOffset(void)
{
    return flashfs.address;
}

uint32_t flashfsGetSectors(void)
{
    return flashfsGetSize() / SPI_FLASH_SEC_SIZE;
}

#endif
