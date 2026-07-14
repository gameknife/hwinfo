// Copyright Leon Freist
// Author Leon Freist <freist@informatik.uni-freiburg.de>

#include <hwinfo/platform.h>

#ifdef HWINFO_WINDOWS

#include <dxgi1_6.h>
#include <devguid.h>
#include <setupapi.h>
#include <windows.h>

#include <algorithm>
#include <cwchar>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "hwinfo/gpu.h"
#include "hwinfo/utils/stringutils.h"
#ifndef __MINGW32__
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "setupapi.lib")
#endif

namespace hwinfo {

namespace {

using PciDeviceId = std::pair<UINT, UINT>;

std::optional<PciDeviceId> parsePciDeviceId(const std::wstring& hardware_id) {
  const auto vendor_pos = hardware_id.find(L"VEN_");
  const auto device_pos = hardware_id.find(L"DEV_");
  if (vendor_pos == std::wstring::npos || device_pos == std::wstring::npos) {
    return std::nullopt;
  }

  wchar_t* vendor_end = nullptr;
  wchar_t* device_end = nullptr;
  const auto vendor = std::wcstoul(hardware_id.c_str() + vendor_pos + 4, &vendor_end, 16);
  const auto device = std::wcstoul(hardware_id.c_str() + device_pos + 4, &device_end, 16);
  if (vendor_end == hardware_id.c_str() + vendor_pos + 4 || device_end == hardware_id.c_str() + device_pos + 4) {
    return std::nullopt;
  }
  return PciDeviceId{static_cast<UINT>(vendor), static_cast<UINT>(device)};
}

std::map<PciDeviceId, std::size_t> getPhysicalGpuCounts() {
  std::map<PciDeviceId, std::size_t> result;
  const HDEVINFO device_info =
      SetupDiGetClassDevsW(&GUID_DEVCLASS_DISPLAY, nullptr, nullptr, DIGCF_PRESENT);
  if (device_info == INVALID_HANDLE_VALUE) {
    return result;
  }

  for (DWORD index = 0;; ++index) {
    SP_DEVINFO_DATA device{};
    device.cbSize = sizeof(device);
    if (!SetupDiEnumDeviceInfo(device_info, index, &device)) {
      break;
    }

    wchar_t hardware_ids[1024]{};
    if (!SetupDiGetDeviceRegistryPropertyW(device_info, &device, SPDRP_HARDWAREID, nullptr,
                                           reinterpret_cast<PBYTE>(hardware_ids), sizeof(hardware_ids), nullptr)) {
      continue;
    }

    // Physical display adapters use PCI hardware IDs. Indirect/virtual display
    // drivers such as Parsec use ROOT\\DISPLAY IDs and are intentionally ignored.
    const auto pci_id = parsePciDeviceId(hardware_ids);
    if (pci_id.has_value()) {
      ++result[*pci_id];
    }
  }

  SetupDiDestroyDeviceInfoList(device_info);
  return result;
}

}  // namespace

// _____________________________________________________________________________________________________________________
std::vector<GPU> getAllGPUs() {
  std::vector<GPU> gpus;

  auto physical_gpu_counts = getPhysicalGpuCounts();

  IDXGIFactory1* pFactory;
  if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pFactory))) return {};

  for (UINT i = 0;; ++i) {
    IDXGIAdapter1* pAdapter = nullptr;
    const HRESULT enum_result = pFactory->EnumAdapters1(i, &pAdapter);
    if (enum_result == DXGI_ERROR_NOT_FOUND) {
      break;
    }
    if (FAILED(enum_result)) {
      continue;
    }

    DXGI_ADAPTER_DESC1 desc;
    pAdapter->GetDesc1(&desc);

    if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
      pAdapter->Release();
      continue;
    }

    if (!physical_gpu_counts.empty()) {
      const PciDeviceId pci_id{desc.VendorId, desc.DeviceId};
      auto physical_gpu = physical_gpu_counts.find(pci_id);
      if (physical_gpu == physical_gpu_counts.end() || physical_gpu->second == 0) {
        pAdapter->Release();
        continue;
      }
      --physical_gpu->second;
    }

    GPU gpu;
    gpu._id = static_cast<int>(i);

    std::wstring ws(desc.Description);
    gpu._name = utils::wstring_to_std_string(ws);

    gpu._dedicated_memory_Bytes = static_cast<int64_t>(desc.DedicatedVideoMemory);
    gpu._shared_memory_Bytes = static_cast<int64_t>(desc.SharedSystemMemory);

    char buffer[10];
    sprintf_s(buffer, "0x%04X", desc.VendorId);
    gpu._vendor_id = buffer;
    sprintf_s(buffer, "0x%04X", desc.DeviceId);
    gpu._device_id = buffer;

    if (desc.VendorId == 0x10DE) {
      gpu._vendor = "NVIDIA";
    } else if (desc.VendorId == 0x1002 || desc.VendorId == 0x1022) {
      gpu._vendor = "AMD";
    } else if (desc.VendorId == 0x8086) {
      gpu._vendor = "Intel";
    } else {
      gpu._vendor = "Unknown";
    }

    gpus.push_back(gpu);
    pAdapter->Release();
  }
  pFactory->Release();
  return gpus;
}

}  // namespace hwinfo

#endif  // HWINFO_WINDOWS
