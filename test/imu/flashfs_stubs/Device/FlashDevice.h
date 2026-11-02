#pragma once
#include "esp_partition.h"
extern esp_partition_t testPartition;
namespace Xsfc::Device {
struct FlashDevice {
  static const esp_partition_t* findPartition() { return &testPartition; }
};
}
