#include <hwinfo/requirements.h>

#include <cstdint>
#include <iostream>
#include <string>

namespace req = hwinfo::requirements;

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: hardware_requirements <gpu-catalog.csv>\n";
    return 2;
  }

  req::GpuCatalog catalog;
  std::string error;
  if (!catalog.loadCsvFile(argv[1], &error)) {
    std::cerr << error << '\n';
    return 1;
  }

  req::MachineRequirements requirement;
  requirement.min_physical_cpu_cores = 4;
  requirement.min_gpu_model = "GeForce RTX 3080 Ti";
  requirement.min_memory_bytes = 8ull * 1024ull * 1024ull * 1024ull;
  requirement.require_solid_state_disk = true;

  const auto report = req::evaluateCurrentMachine(requirement, catalog);
  std::cout << "overall: " << req::to_string(report.overall) << '\n'
            << "cpu: " << req::to_string(report.cpu.status) << " (" << report.cpu.detected_physical_cores
            << " physical cores)\n"
            << "gpu: " << req::to_string(report.gpu.status) << " (" << report.gpu.detected_model << ")\n"
            << "memory: " << req::to_string(report.memory.status) << " (" << report.memory.detected_bytes
            << " bytes)\n"
            << "disk: " << req::to_string(report.disk.status) << " (" << report.disk.solid_state_count
            << " SSDs)\n";
  return report.overall == req::EvaluationStatus::FAILED ? 1 : 0;
}
