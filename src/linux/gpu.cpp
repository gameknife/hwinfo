
// Copyright Leon Freist
// Author Leon Freist <freist@informatik.uni-freiburg.de>

#include <hwinfo/platform.h>

#ifdef HWINFO_UNIX

#include "hwinfo/gpu.h"
#include "hwinfo/utils/stringutils.h"
#include "hwinfo/utils/unit.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace hwinfo {

namespace {

struct PciNames {
  std::string vendor;
  std::string device;
};

std::string normalizePciId(std::string id) {
  utils::strip(id);
  if (utils::starts_with(id, "0x")) {
    id.erase(0, 2);
  }
  std::transform(id.begin(), id.end(), id.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return id;
}

PciNames lookupPciNames(const std::string& vendor_id, const std::string& device_id) {
  static constexpr std::array<const char*, 2> database_paths = {
      "/usr/share/hwdata/pci.ids",
      "/usr/share/misc/pci.ids",
  };

  const std::string wanted_vendor = normalizePciId(vendor_id);
  const std::string wanted_device = normalizePciId(device_id);
  PciNames names;

  for (const char* database_path : database_paths) {
    std::ifstream database(database_path);
    if (!database) {
      continue;
    }

    bool in_vendor = false;
    std::string line;
    while (std::getline(database, line)) {
      if (line.empty() || line[0] == '#') {
        continue;
      }

      if (line[0] != '\t') {
        in_vendor = line.size() >= 4 && normalizePciId(line.substr(0, 4)) == wanted_vendor;
        if (in_vendor) {
          names.vendor = line.substr(4);
          utils::strip(names.vendor);
        } else if (!names.vendor.empty()) {
          break;
        }
        continue;
      }

      if (!in_vendor || line.size() < 5 || line[1] == '\t') {
        continue;
      }
      if (normalizePciId(line.substr(1, 4)) == wanted_device) {
        names.device = line.substr(5);
        utils::strip(names.device);
        return names;
      }
    }
    if (!names.vendor.empty()) {
      return names;
    }
  }

  if (wanted_vendor == "10de") {
    names.vendor = "NVIDIA";
  } else if (wanted_vendor == "1002" || wanted_vendor == "1022") {
    names.vendor = "AMD";
  } else if (wanted_vendor == "8086") {
    names.vendor = "Intel";
  }
  return names;
}

}  // namespace

// _____________________________________________________________________________________________________________________
std::string read_drm_by_path(const std::string& path) {
  std::ifstream f_drm(path);
  if (!f_drm) {
    return "";
  }
  std::string ret;
  getline(f_drm, ret);
  return ret;
}

// _____________________________________________________________________________________________________________________
std::vector<std::uint64_t> get_frequencies(const std::string drm_path) {
  // {min, current, max}
  std::vector<std::uint64_t> freqs(3, 0);
  try {
    freqs[0] =
        std::stoull(read_drm_by_path(drm_path + "gt_min_freq_mhz")) * static_cast<std::uint64_t>(unit::SiPrefix::MEGA);
  } catch (const std::invalid_argument& e) {
    freqs[0] = 0;
  }
  try {
    freqs[1] =
        std::stoull(read_drm_by_path(drm_path + "gt_cur_freq_mhz")) * static_cast<std::uint64_t>(unit::SiPrefix::MEGA);
  } catch (const std::invalid_argument& e) {
    freqs[0] = 0;
  }
  try {
    freqs[2] =
        std::stoull(read_drm_by_path(drm_path + "gt_max_freq_mhz")) * static_cast<std::uint64_t>(unit::SiPrefix::MEGA);
  } catch (const std::invalid_argument& e) {
    freqs[0] = 0;
  }
  return freqs;
}

// _____________________________________________________________________________________________________________________
std::vector<GPU> getAllGPUs() {
  std::vector<GPU> gpus{};
  int id = 0;
  while (true) {
    GPU gpu;
    gpu._id = id;
    std::filesystem::path path("/sys/class/drm/card" + std::to_string(id));
    if (!std::filesystem::exists(path)) {
      if (id > 2) {
        break;
      }
      id++;
      continue;
    }
    gpu._vendor_id = read_drm_by_path(path / "device/vendor");
    gpu._device_id = read_drm_by_path(path / "device/device");
    if (gpu._vendor_id.empty() || gpu._device_id.empty()) {
      id++;
      continue;
    }
    const auto names = lookupPciNames(gpu._vendor_id, gpu._device_id);
    gpu._vendor = names.vendor.empty() ? gpu._vendor_id : names.vendor;
    gpu._name = names.device.empty() ? gpu._device_id : names.device;
    auto frequencies = get_frequencies(path);
    gpu._frequency_hz = frequencies[2];
    gpus.push_back(std::move(gpu));
    id++;
  }
  return gpus;
}

}  // namespace hwinfo

#endif  // HWINFO_UNIX
