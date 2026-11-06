#include "Utils/Storage.h"

// Host calibration must never touch real EEPROM or silently persist settings.
Xsfc::StorageResult Xsfc::Utils::Storage::save(const Xsfc::ModelConfig&) {
  return Xsfc::STORAGE_SAVE_ERROR;
}
