#pragma once
#include "json.h"
#include <cstdint>

namespace rc {
struct UpdateInfo {
  std::wstring version;
  std::wstring installerUrl;
  std::wstring sha256;
  uint64_t size{};
  std::wstring notes;
};

struct UpdateResult {
  bool ok{};
  bool available{};
  UpdateInfo info;
  std::wstring error;
};

std::wstring CurrentVersion();
UpdateResult CheckForUpdate();
bool DownloadAndVerifyUpdate(const UpdateInfo& info, std::filesystem::path& installer, std::wstring& error);
}
