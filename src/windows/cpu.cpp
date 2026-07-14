// Copyright (c) Leon Freist <freist@informatik.uni-freiburg.de>
// This software is part of HWBenchmark

#include <hwinfo/platform.h>

#ifdef HWINFO_WINDOWS

#include <hwinfo/cpu.h>
#include <hwinfo/utils/unit.h>
#include <hwinfo/utils/win_registry.h>
#include <intrin.h>

#include <string>
#include <vector>

inline int countSetBits(unsigned __int64 mask) {
#if defined(_M_X64) || defined(__x86_64__)
  return static_cast<int>(__popcnt64(mask));
#else
  return static_cast<int>(__popcnt(static_cast<unsigned int>(mask & 0xFFFFFFFF)) +
                          __popcnt(static_cast<unsigned int>(mask >> 32)));
#endif
}

namespace hwinfo {
// _____________________________________________________________________________________________________________________
std::vector<CPU> getAllCPUs() {
  std::vector<CPU> cpus;
  CPU local_cpu;
  local_cpu._id = 0;

  std::wstring reg_cpu_path = L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";
  local_cpu._modelName =
      internal::utils::getRegistryValue<std::string>(HKEY_LOCAL_MACHINE, reg_cpu_path, L"ProcessorNameString");
  local_cpu._vendor =
      internal::utils::getRegistryValue<std::string>(HKEY_LOCAL_MACHINE, reg_cpu_path, L"VendorIdentifier");

  DWORD bufferSize = 0;
  GetLogicalProcessorInformationEx(RelationAll, nullptr, &bufferSize);
  std::vector<BYTE> buffer(bufferSize);

  if (!GetLogicalProcessorInformationEx(RelationAll, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buffer.data(),
                                        &bufferSize)) {
    return {};
  }

  std::vector<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX> coreEntries;
  std::vector<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX> cacheEntries;

  unsigned char* ptr = buffer.data();
  while (ptr < buffer.data() + bufferSize) {
    auto info = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)ptr;
    if (info->Relationship == RelationProcessorCore) coreEntries.push_back(info);
    if (info->Relationship == RelationCache) cacheEntries.push_back(info);
    ptr += info->Size;
  }

  auto regular_frequency =
      internal::utils::getRegistryValue<int64_t>(HKEY_LOCAL_MACHINE, reg_cpu_path, L"~MHz") * unit::SiPrefix::MEGA;

  for (size_t i = 0; i < coreEntries.size(); ++i) {
    auto cInfo = coreEntries[i];
    CPU::Core core{};
    core.id = i;
    core.regular_frequency_hz = regular_frequency;
    core.max_frequency_hz = regular_frequency;

    // SMT is true if logical threads > 1 for this physical core
    int threadsInThisCore = countSetBits(cInfo->Processor.GroupMask[0].Mask);
    core.smt = (threadsInThisCore > 1);

    local_cpu._numPhysicalCores++;
    local_cpu._numLogicalCores += threadsInThisCore;

    // Initialize cache vector [L1 Data, L1 Instruction, L2, L3]
    core.cache = {0, 0, 0, 0};

    for (auto cache : cacheEntries) {
      if ((cInfo->Processor.GroupMask[0].Mask & cache->Cache.GroupMask.Mask) != 0) {
        uint8_t level = cache->Cache.Level;
        auto type = cache->Cache.Type;

        if (level == 1) {
          if (type == CacheData) {
            core.cache.l1_data = cache->Cache.CacheSize;
          } else if (type == CacheInstruction) {
            core.cache.l1_instruction = cache->Cache.CacheSize;
          } else if (type == CacheUnified) {
            // Some CPUs have unified L1 (rare but possible)
            core.cache.l1_data = core.cache.l1_instruction = cache->Cache.CacheSize;
          }
        } else if (level == 2) {
          core.cache.l2 = cache->Cache.CacheSize;
        } else if (level == 3) {
          core.cache.l3 = cache->Cache.CacheSize;
        }
      }
    }
    local_cpu._cores.push_back(core);
  }

  cpus.emplace_back(local_cpu);
  return cpus;
}

}  // namespace hwinfo

#endif  // HWINFO_WINDOWS
