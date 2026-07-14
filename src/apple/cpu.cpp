// Copyright (c) Leon Freist <freist@informatik.uni-freiburg.de>
// This software is part of HWBenchmark

#include "hwinfo/platform.h"

#ifdef HWINFO_APPLE

#include <sys/sysctl.h>

#include <string>
#include <vector>

#include "hwinfo/cpu.h"
#include "hwinfo/utils/sysctl.h"

namespace hwinfo {

// Helper functions to reduce code duplication
namespace {

// Check if the system is running on Apple Silicon
bool isAppleSilicon() {
  auto machine = utils::getSysctlString("hw.machine");
  return machine.find("arm64") != std::string::npos;
}

bool isPowerPC() {
  int cputype = utils::getSysctlValue<int>("hw.cputype", -1);
  return cputype == 18; // 18 is CPU_TYPE_POWERPC
}

}  // anonymous namespace

// _____________________________________________________________________________________________________________________
std::string getVendor() {
#if defined(HWINFO_X86)
  std::string vendor;
  uint32_t regs[4]{0};
  cpuid::cpuid(0, 0, regs);
  vendor += std::string((const char*)&regs[1], 4);
  vendor += std::string((const char*)&regs[3], 4);
  vendor += std::string((const char*)&regs[2], 4);
#else
  // Try to get vendor from sysctl
  auto vendor = utils::getSysctlString("machdep.cpu.vendor", "<unknown>");

  // Check if this is Apple Silicon
  if (vendor == "<unknown>" && isAppleSilicon()) {
    return "Apple";
  }

  if (vendor == "<unknown>" && isPowerPC()) {
    return "IBM";
  }
#endif
  return vendor;
}

// _____________________________________________________________________________________________________________________
std::string getModelName() {
#if defined(HWINFO_X86)
  std::string model;
  uint32_t regs[4]{};
  for (unsigned i = 0x80000002; i < 0x80000005; ++i) {
    cpuid::cpuid(i, 0, regs);
    for (auto c : std::string((const char*)&regs[0], 4)) {
      if (std::isalnum(c) || c == '(' || c == ')' || c == '@' || c == ' ' || c == '-' || c == '.') {
        model += c;
      }
    }
    for (auto c : std::string((const char*)&regs[1], 4)) {
      if (std::isalnum(c) || c == '(' || c == ')' || c == '@' || c == ' ' || c == '-' || c == '.') {
        model += c;
      }
    }
    for (auto c : std::string((const char*)&regs[2], 4)) {
      if (std::isalnum(c) || c == '(' || c == ')' || c == '@' || c == ' ' || c == '-' || c == '.') {
        model += c;
      }
    }
    for (auto c : std::string((const char*)&regs[3], 4)) {
      if (std::isalnum(c) || c == '(' || c == ')' || c == '@' || c == ' ' || c == '-' || c == '.') {
        model += c;
      }
    }
  }
#else
  std::string model = utils::getSysctlString("machdep.cpu.brand_string", "<unknown>");
  if (model == "<unknown>" && isAppleSilicon()) {
    model = "Apple Silicon";
  }
  if (model == "<unknown>" && isPowerPC()) {
    int cpusubtype = utils::getSysctlValue<int>("hw.cpusubtype", -1);
    switch (cpusubtype) {
      case 100: model = "PowerPC G5 (970)"; break;
      case 11: model = "PowerPC G4 (7450)"; break;
      case 10: model = "PowerPC G4 (7400)"; break;
      case 9: model = "PowerPC G3 (750)"; break;
      case 1: model = "PowerPC 601"; break;
      default: model = "PowerPC (unknown subtype: " + std::to_string(cpusubtype) + ")";
    }
  }
#endif
  return model;
}

int getNumLogicalCores();

// _____________________________________________________________________________________________________________________
int getNumPhysicalCores() { return utils::getSysctlValue<int>("hw.physicalcpu", 0); }

// _____________________________________________________________________________________________________________________
int getNumLogicalCores() { return utils::getSysctlValue<int>("hw.logicalcpu", 0); }

[[maybe_unused]] int64_t getL1CacheSize_Bytes() { return utils::getSysctlValue<int64_t>("hw.l1dcachesize", -1); }

[[maybe_unused]] int64_t getL2CacheSize_Bytes() { return utils::getSysctlValue<int64_t>("hw.l2cachesize", -1); }

[[maybe_unused]] int64_t getL3CacheSize_Bytes() { return utils::getSysctlValue<int64_t>("hw.l3cachesize", -1); }

// _____________________________________________________________________________________________________________________
std::vector<CPU> getAllCPUs() {
  std::vector<CPU> cpus;
  CPU cpu;

  cpu._vendor = getVendor();
  cpu._modelName = getModelName();
  cpu._numPhysicalCores = getNumPhysicalCores();
  cpu._numLogicalCores = getNumLogicalCores();

  cpus.push_back(cpu);

  return cpus;
}

}  // namespace hwinfo

#endif  // HWINFO_APPLE
