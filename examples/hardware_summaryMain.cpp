// Copyright Leon Freist
// Author Leon Freist <freist@informatik.uni-freiburg.de>

#include <hwinfo/hwinfo.h>

#include <iostream>

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
              << "  solid state: " << (disk.is_solid_state() ? "yes" : "no") << '\n';
  }

  return 0;
}
