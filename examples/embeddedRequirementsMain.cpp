#include <hwinfo/requirements.h>

#include <hwinfo_local_requirements.h>

#include <iostream>
#include <sstream>
#include <string>

namespace req = hwinfo::requirements;
namespace local = hwinfo::local_requirements;

int main() {
  std::istringstream catalog_stream(std::string(local::gpu_catalog_csv, local::gpu_catalog_csv_size));
  req::GpuCatalog catalog;
  std::string error;
  if (!catalog.loadCsv(catalog_stream, &error)) {
    std::cerr << "embedded GPU catalog is invalid: " << error << '\n';
    return 3;
  }

  req::MachineRequirements requirement;
  requirement.min_physical_cpu_cores = local::min_physical_cpu_cores;
  requirement.min_gpu_model = local::min_gpu_model;
  requirement.allow_unrecognized_gpu = local::allow_unrecognized_gpu;
  requirement.min_memory_bytes = local::min_memory_bytes;
  requirement.require_solid_state_disk = local::require_solid_state_disk;

  const auto report = req::evaluateCurrentMachine(requirement, catalog);
  const std::string displayed_gpu =
      report.gpu.passed_by_unrecognized_policy && !report.gpu.unresolved_models.empty()
          ? report.gpu.unresolved_models.front() + " (unrecognized model allowed)"
          : report.gpu.detected_canonical_model;
  std::cout << "overall: " << req::to_string(report.overall) << '\n'
            << "cpu: " << req::to_string(report.cpu.status) << " (required "
            << report.cpu.required_physical_cores << ", detected " << report.cpu.detected_physical_cores << ")\n"
            << "gpu: " << req::to_string(report.gpu.status) << " (required "
            << report.gpu.required_canonical_model << ", detected " << displayed_gpu << ")\n"
            << "memory: " << req::to_string(report.memory.status) << " (required " << report.memory.required_bytes
            << ", detected " << report.memory.detected_bytes << " bytes)\n"
            << "disk: " << req::to_string(report.disk.status) << " (detected " << report.disk.solid_state_count
            << " SSDs)\n";

  switch (report.overall) {
    case req::EvaluationStatus::PASSED:
      return 0;
    case req::EvaluationStatus::FAILED:
      return 1;
    case req::EvaluationStatus::UNKNOWN:
      return 2;
  }
  return 3;
}
