#pragma once
#include <cstdint>
#include <cstddef>
struct esp_partition_t { uint32_t size; };
using esp_err_t = int;
constexpr int ESP_OK = 0;
int esp_partition_write_raw(const esp_partition_t*, size_t, const void*, size_t);
int esp_partition_read_raw(const esp_partition_t*, size_t, void*, size_t);
int esp_partition_erase_range(const esp_partition_t*, size_t, size_t);
