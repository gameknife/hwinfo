// Copyright Leon Freist
// Author Leon Freist <freist@informatik.uni-freiburg.de>

#include <hwinfo/hwinfo.h>
#include <hwinfo/utils/unit.h>

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>

#ifdef HWINFO_WINDOWS
#include <windows.h>
#include <winioctl.h>
#endif

namespace {

std::optional<bool> isSolidState(const hwinfo::Disk& disk) {
  if (disk.disk_interface() == hwinfo::Disk::Interface::NVME) {
    return true;
  }

#ifdef HWINFO_WINDOWS
  const std::string device_path = "\\\\.\\PhysicalDrive" + std::to_string(disk.id());
  const HANDLE device =
      CreateFileA(device_path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
  if (device == INVALID_HANDLE_VALUE) {
    return std::nullopt;
  }

  STORAGE_PROPERTY_QUERY query{};
  query.PropertyId = StorageDeviceSeekPenaltyProperty;
  query.QueryType = PropertyStandardQuery;

  DEVICE_SEEK_PENALTY_DESCRIPTOR descriptor{};
  DWORD bytes_returned = 0;
  const BOOL success = DeviceIoControl(device, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &descriptor,
                                       sizeof(descriptor), &bytes_returned, nullptr);
  CloseHandle(device);

  if (!success || bytes_returned < sizeof(descriptor)) {
    return std::nullopt;
  }
  return descriptor.IncursSeekPenalty == FALSE;
#else
  return std::nullopt;
#endif
}

const char* yesNoUnknown(const std::optional<bool> value) {
  if (!value.has_value()) {
    return "unknown";
  }
  return *value ? "yes" : "no";
}

}  // namespace

int main() {
  const auto cpus = hwinfo::getAllCPUs();
  for (const auto& cpu : cpus) {
    std::cout << "CPU " << cpu.id() << ":\n"
              << "  model: " << cpu.modelName() << '\n'
              << "  physical cores: " << cpu.numPhysicalCores() << '\n'
              << "  logical cores: " << cpu.numLogicalCores() << '\n';
  }

  const auto gpus = hwinfo::getAllGPUs();
  for (const auto& gpu : gpus) {
    std::cout << "GPU " << gpu.id() << ":\n" << "  model: " << gpu.name() << '\n';
  }

  const hwinfo::Memory memory;
  std::cout << "RAM:\n"
            << "  total: " << hwinfo::unit::unit_prefix_to(memory.size(), hwinfo::unit::IECPrefix::GIBI) << " GiB\n";

  const auto disks = hwinfo::getAllDisks();
  for (const auto& disk : disks) {
    std::cout << "Disk " << disk.id() << ":\n"
              << "  model: " << disk.model() << '\n'
              << "  solid state: " << yesNoUnknown(isSolidState(disk)) << '\n';
  }

  return 0;
}
