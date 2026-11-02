#pragma once
#include "esp_partition.h"
using esp_partition_mmap_handle_t = unsigned;
constexpr int ESP_PARTITION_MMAP_DATA = 0;
int esp_partition_mmap(const esp_partition_t*, size_t, size_t, int, const void**, esp_partition_mmap_handle_t*);
