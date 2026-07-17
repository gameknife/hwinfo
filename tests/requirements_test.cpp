#include <hwinfo/requirements.h>

#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace req = hwinfo::requirements;

namespace {

void expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

req::GpuCatalog makeCatalog() {
  std::istringstream csv(
      "# source=unit test\n"
      "# retrieved_at=2026-07-14T00:00:00Z\n"
      "canonical_model,score,rank,vendor,aliases,vendor_id,device_id\n"
      "GeForce GTX 1060,100,4,NVIDIA,GTX 1060,,\n"
      "NVIDIA GeForce RTX 3050,140,7,NVIDIA,GeForce RTX 3050 6GB,,\n"
      "GeForce RTX 3050 8GB,150,6,NVIDIA,,,\n"
      "GeForce RTX 3060,200,2,NVIDIA,,,\n"
      "GeForce RTX 3060 Laptop GPU,160,3,NVIDIA,,,\n"
      "GeForce RTX 4050 6GB,240,9,NVIDIA,,,\n"
      "GeForce RTX 4050 8GB,250,8,NVIDIA,,,\n"
      "Radeon RX 580,90,5,AMD,,,\n"
      "Intel UHD Graphics 630,20,6,Intel,,,\n"
      "GeForce RTX 4090,400,1,NVIDIA,,10de,2684\n");
  req::GpuCatalog catalog;
  std::string error;
  expect(catalog.loadCsv(csv, &error), "catalog should load: " + error);
  return catalog;
}

req::HardwareSnapshot capableSnapshot() {
  req::HardwareSnapshot snapshot;
  snapshot.cpus = {{"CPU socket 0", 4, 8}, {"CPU socket 1", 4, 8}};
  snapshot.gpus = {{"Intel", "Intel(R) UHD Graphics 630", "0x8086", "0x3e92"},
                   {"NVIDIA", "NVIDIA GeForce RTX 3060", "0x10de", "0x2503"}};
  snapshot.memory_bytes = 16ull * 1024ull * 1024ull * 1024ull;
  snapshot.disks = {{"HDD", hwinfo::Disk::MediaType::ROTATIONAL},
                    {"NVMe", hwinfo::Disk::MediaType::SOLID_STATE}};
  return snapshot;
}

void testCatalogLookup() {
  const auto catalog = makeCatalog();
  expect(catalog.source() == "unit test", "catalog source metadata should load");
  expect(catalog.entries().size() == 10, "all catalog rows should load");

  const auto relaxed = catalog.lookup("RTX 3060");
  expect(relaxed.status == req::GpuLookupStatus::MATCHED, "a unique relaxed model should match");
  expect(relaxed.entry.canonical_model == "GeForce RTX 3060", "desktop model should not match laptop variant");

  const auto driver_name = catalog.lookup("NVIDIA GeForce RTX 3060 Laptop GPU", "NVIDIA");
  expect(driver_name.status == req::GpuLookupStatus::MATCHED, "driver prefixes should normalize");
  expect(driver_name.entry.score == 160, "laptop score should remain distinct");

  const auto gpuinfo_name = catalog.lookup("NVIDIA GeForce RTX 3050", "NVIDIA");
  expect(gpuinfo_name.status == req::GpuLookupStatus::MATCHED, "a gpuinfo canonical model should match directly");
  expect(gpuinfo_name.entry.canonical_model == "NVIDIA GeForce RTX 3050",
         "gpuinfo model names should remain canonical");

  const auto passmark_alias = catalog.lookup("GeForce RTX 3050 6GB", "NVIDIA");
  expect(passmark_alias.status == req::GpuLookupStatus::MATCHED, "the original PassMark model should remain an alias");
  expect(passmark_alias.entry.canonical_model == "NVIDIA GeForce RTX 3050",
         "a PassMark alias should resolve to the gpuinfo canonical model");

  const auto fuzzy_driver_name = catalog.lookup("NVIDIA GeForce RTX 4050", "NVIDIA");
  expect(fuzzy_driver_name.status == req::GpuLookupStatus::MATCHED,
         "a driver model missing a catalog qualifier should fuzzy-match");
  expect(fuzzy_driver_name.entry.canonical_model == "GeForce RTX 4050 6GB",
         "an equally close fuzzy match should conservatively use the lowest-scoring variant");

  const auto too_broad = catalog.lookup("GeForce RTX", "NVIDIA");
  expect(too_broad.status == req::GpuLookupStatus::NOT_FOUND,
         "a family name without a model number should not fuzzy-match");

  const auto pci = catalog.lookup("unrecognized name", "NVIDIA", "0x10DE", "0x2684");
  expect(pci.status == req::GpuLookupStatus::MATCHED, "known PCI IDs should take precedence over the model string");
  expect(pci.entry.canonical_model == "GeForce RTX 4090", "PCI lookup should return the mapped model");
}

void testPassingEvaluation() {
  const auto catalog = makeCatalog();
  req::MachineRequirements requirement;
  requirement.min_physical_cpu_cores = 8;
  requirement.min_gpu_model = "GTX 1060";
  requirement.min_memory_bytes = 16ull * 1024ull * 1024ull * 1024ull;
  requirement.require_solid_state_disk = true;

  const auto report = req::evaluate(capableSnapshot(), requirement, catalog);
  expect(report.overall == req::EvaluationStatus::PASSED, "all satisfied requirements should pass");
  expect(report.cpu.detected_physical_cores == 8, "physical cores should be summed across sockets");
  expect(report.gpu.detected_canonical_model == "GeForce RTX 3060", "the fastest matched GPU should be selected");
  expect(report.disk.solid_state_count == 1, "any detected SSD should satisfy the disk requirement");
}

void testFailingEvaluation() {
  const auto catalog = makeCatalog();
  auto snapshot = capableSnapshot();
  snapshot.cpus.resize(1);
  snapshot.gpus = {{"NVIDIA", "NVIDIA GeForce GTX 1060", "", ""}};
  snapshot.memory_bytes = 8ull * 1024ull * 1024ull * 1024ull;
  snapshot.disks = {{"HDD", hwinfo::Disk::MediaType::ROTATIONAL}};

  req::MachineRequirements requirement;
  requirement.min_physical_cpu_cores = 8;
  requirement.min_gpu_model = "RTX 3060";
  requirement.min_memory_bytes = 16ull * 1024ull * 1024ull * 1024ull;
  requirement.require_solid_state_disk = true;

  const auto report = req::evaluate(snapshot, requirement, catalog);
  expect(report.overall == req::EvaluationStatus::FAILED, "known insufficient hardware should fail");
  expect(report.cpu.status == req::EvaluationStatus::FAILED, "insufficient known CPU cores should fail");
  expect(report.gpu.status == req::EvaluationStatus::FAILED, "a lower catalog score should fail");
  expect(report.memory.status == req::EvaluationStatus::FAILED, "insufficient memory should fail");
  expect(report.disk.status == req::EvaluationStatus::FAILED, "all-rotational storage should fail");
}

void testUnknownEvaluation() {
  const auto catalog = makeCatalog();
  req::HardwareSnapshot snapshot;
  snapshot.gpus = {{"Unknown", "Future GPU 9000", "", ""}};
  snapshot.disks = {{"Mystery disk", hwinfo::Disk::MediaType::UNKNOWN}};

  req::MachineRequirements requirement;
  requirement.min_physical_cpu_cores = 4;
  requirement.min_gpu_model = "GTX 1060";
  requirement.min_memory_bytes = 8ull * 1024ull * 1024ull * 1024ull;
  requirement.require_solid_state_disk = true;

  const auto report = req::evaluate(snapshot, requirement, catalog);
  expect(report.overall == req::EvaluationStatus::UNKNOWN, "missing detection data should be unknown");
  expect(report.cpu.status == req::EvaluationStatus::UNKNOWN, "missing CPUs should be unknown");
  expect(report.gpu.status == req::EvaluationStatus::UNKNOWN, "an unrecognized GPU should be unknown");
  expect(report.memory.status == req::EvaluationStatus::UNKNOWN, "zero memory should be unknown");
  expect(report.disk.status == req::EvaluationStatus::UNKNOWN, "unknown media should not be treated as an HDD");
}

void testUnknownGpuDoesNotHideKnownPass() {
  const auto catalog = makeCatalog();
  auto snapshot = capableSnapshot();
  snapshot.gpus.push_back({"Unknown", "Future GPU 9000", "", ""});

  req::MachineRequirements requirement;
  requirement.min_gpu_model = "GTX 1060";
  const auto report = req::evaluate(snapshot, requirement, catalog);
  expect(report.gpu.status == req::EvaluationStatus::PASSED,
         "a known passing GPU should pass even when another GPU is unresolved");
}

void testUnrecognizedGpuPolicy() {
  const auto catalog = makeCatalog();
  req::MachineRequirements requirement;
  requirement.min_gpu_model = "RTX 3060";
  requirement.allow_unrecognized_gpu = true;

  req::HardwareSnapshot future_gpu;
  future_gpu.gpus = {{"New Vendor", "Future GPU 9000", "", ""}};
  const auto future_report = req::evaluate(future_gpu, requirement, catalog);
  expect(future_report.gpu.status == req::EvaluationStatus::PASSED,
         "an unrecognized detected GPU should pass when the policy is enabled");
  expect(future_report.gpu.passed_by_unrecognized_policy,
         "the report should say that the unrecognized-GPU policy caused the pass");

  req::HardwareSnapshot no_gpu;
  const auto no_gpu_report = req::evaluate(no_gpu, requirement, catalog);
  expect(no_gpu_report.gpu.status == req::EvaluationStatus::UNKNOWN,
         "the policy must not pass when no GPU was detected at all");

  req::HardwareSnapshot known_weak_gpu;
  known_weak_gpu.gpus = {{"NVIDIA", "GeForce GTX 1060", "", ""}};
  const auto weak_report = req::evaluate(known_weak_gpu, requirement, catalog);
  expect(weak_report.gpu.status == req::EvaluationStatus::FAILED,
         "a known GPU below the requirement should still fail");
}

void testAmbiguousAliases() {
  req::GpuCatalog catalog;
  req::GpuCatalogEntry first{"First GPU", "Vendor", 10, 2, {"Shared Alias"}, "", ""};
  req::GpuCatalogEntry second{"Second GPU", "Vendor", 20, 1, {"Shared Alias"}, "", ""};
  expect(catalog.addEntry(first), "first entry should be accepted");
  expect(catalog.addEntry(second), "second entry should be accepted");
  const auto lookup = catalog.lookup("Shared Alias", "Vendor");
  expect(lookup.status == req::GpuLookupStatus::AMBIGUOUS, "ambiguous aliases must not pick a winner");
  expect(lookup.candidates.size() == 2, "ambiguous lookup should expose candidates");
}

}  // namespace

int main() {
  try {
    testCatalogLookup();
    testPassingEvaluation();
    testFailingEvaluation();
    testUnknownEvaluation();
    testUnknownGpuDoesNotHideKnownPass();
    testUnrecognizedGpuPolicy();
    testAmbiguousAliases();
  } catch (const std::exception& error) {
    std::cerr << "requirements_test failed: " << error.what() << '\n';
    return 1;
  }
  std::cout << "requirements_test passed\n";
  return 0;
}
