// Copyright Leon Freist
// Optional hardware-requirement evaluation layer for hwinfo.

#pragma once

#include <hwinfo/hwinfo.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace hwinfo::requirements {

enum class EvaluationStatus {
  PASSED,
  FAILED,
  UNKNOWN
};

inline const char* to_string(EvaluationStatus status) {
  switch (status) {
    case EvaluationStatus::PASSED:
      return "passed";
    case EvaluationStatus::FAILED:
      return "failed";
    case EvaluationStatus::UNKNOWN:
      return "unknown";
  }
  return "unknown";
}

struct CpuSnapshot {
  std::string model;
  std::uint64_t physical_cores = 0;
  std::uint64_t logical_cores = 0;
};

struct GpuSnapshot {
  std::string vendor;
  std::string model;
  std::string vendor_id;
  std::string device_id;
};

struct DiskSnapshot {
  std::string model;
  Disk::MediaType media_type = Disk::MediaType::UNKNOWN;
};

struct HardwareSnapshot {
  std::vector<CpuSnapshot> cpus;
  std::vector<GpuSnapshot> gpus;
  std::uint64_t memory_bytes = 0;
  std::vector<DiskSnapshot> disks;
};

inline HardwareSnapshot collectHardwareSnapshot() {
  HardwareSnapshot snapshot;

  for (const auto& cpu : getAllCPUs()) {
    snapshot.cpus.push_back({cpu.modelName(), cpu.numPhysicalCores(), cpu.numLogicalCores()});
  }
  for (const auto& gpu : getAllGPUs()) {
    snapshot.gpus.push_back({gpu.vendor(), gpu.name(), gpu.vendor_id(), gpu.device_id()});
  }

  const Memory memory;
  snapshot.memory_bytes = memory.size();

  for (const auto& disk : getAllDisks()) {
    snapshot.disks.push_back({disk.model(), disk.media_type()});
  }
  return snapshot;
}

struct MachineRequirements {
  std::uint64_t min_physical_cpu_cores = 0;
  std::string min_gpu_model;
  bool allow_unrecognized_gpu = false;
  std::uint64_t min_memory_bytes = 0;
  bool require_solid_state_disk = false;
};

struct CpuEvaluation {
  EvaluationStatus status = EvaluationStatus::UNKNOWN;
  std::uint64_t required_physical_cores = 0;
  std::uint64_t detected_physical_cores = 0;
  std::uint64_t detected_logical_cores = 0;
  std::string detail;
};

struct GpuEvaluation {
  EvaluationStatus status = EvaluationStatus::UNKNOWN;
  std::string catalog_source;
  std::string catalog_retrieved_at;
  std::string required_model;
  std::string required_canonical_model;
  std::uint64_t required_score = 0;
  std::string detected_model;
  std::string detected_canonical_model;
  std::uint64_t detected_score = 0;
  std::vector<std::string> unresolved_models;
  std::vector<std::string> candidates;
  bool passed_by_unrecognized_policy = false;
  std::string detail;
};

struct MemoryEvaluation {
  EvaluationStatus status = EvaluationStatus::UNKNOWN;
  std::uint64_t required_bytes = 0;
  std::uint64_t detected_bytes = 0;
  std::string detail;
};

struct DiskEvaluation {
  EvaluationStatus status = EvaluationStatus::UNKNOWN;
  bool solid_state_required = false;
  std::size_t solid_state_count = 0;
  std::size_t rotational_count = 0;
  std::size_t unknown_count = 0;
  std::string detail;
};

struct EvaluationReport {
  EvaluationStatus overall = EvaluationStatus::UNKNOWN;
  CpuEvaluation cpu;
  GpuEvaluation gpu;
  MemoryEvaluation memory;
  DiskEvaluation disk;

  HWI_NODISCARD bool passed() const { return overall == EvaluationStatus::PASSED; }
};

struct GpuCatalogEntry {
  std::string canonical_model;
  std::string vendor;
  std::uint64_t score = 0;
  std::uint32_t rank = 0;
  std::vector<std::string> aliases;
  std::string vendor_id;
  std::string device_id;
};

enum class GpuLookupStatus {
  MATCHED,
  NOT_FOUND,
  AMBIGUOUS
};

struct GpuLookupResult {
  GpuLookupStatus status = GpuLookupStatus::NOT_FOUND;
  GpuCatalogEntry entry;
  std::vector<std::string> candidates;
};

namespace detail {

inline std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return {};
  }
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

inline std::string ascii_lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

inline std::vector<std::string> tokenize(const std::string& value) {
  std::vector<std::string> tokens;
  std::string token;
  for (unsigned char c : value) {
    if (std::isalnum(c)) {
      token.push_back(static_cast<char>(std::tolower(c)));
    } else if (!token.empty()) {
      tokens.push_back(std::move(token));
      token.clear();
    }
  }
  if (!token.empty()) {
    tokens.push_back(std::move(token));
  }
  return tokens;
}

inline bool is_company_noise(const std::string& token) {
  static const std::vector<std::string> noise = {
      "amd", "ati", "nvidia", "intel", "advanced", "micro", "devices", "corporation", "corp", "inc", "tm", "r"};
  return std::find(noise.begin(), noise.end(), token) != noise.end();
}

inline bool is_relaxed_noise(const std::string& token) {
  static const std::vector<std::string> noise = {"geforce", "radeon", "graphics", "gpu"};
  return is_company_noise(token) || std::find(noise.begin(), noise.end(), token) != noise.end();
}

inline std::string join_model_tokens(const std::vector<std::string>& tokens, bool relaxed) {
  std::string result;
  for (const auto& token : tokens) {
    if ((relaxed ? is_relaxed_noise(token) : is_company_noise(token)) || token == "with" || token == "design") {
      continue;
    }
    if (!result.empty()) {
      result.push_back(' ');
    }
    result += token;
  }
  return result;
}

inline std::string normalize_model(const std::string& value, bool relaxed) {
  return join_model_tokens(tokenize(value), relaxed);
}

inline bool fuzzy_model_prefix_distance(const std::string& first, const std::string& second,
                                        std::size_t& distance) {
  const auto first_tokens = tokenize(first);
  const auto second_tokens = tokenize(second);
  const auto& shorter = first_tokens.size() <= second_tokens.size() ? first_tokens : second_tokens;
  const auto& longer = first_tokens.size() <= second_tokens.size() ? second_tokens : first_tokens;
  if (shorter.size() < 2 || shorter.size() == longer.size() ||
      !std::equal(shorter.begin(), shorter.end(), longer.begin())) {
    return false;
  }

  // A family name such as "GeForce RTX" is too broad to identify a model. Require
  // at least one model token containing a digit before accepting a prefix match.
  const bool has_model_number = std::any_of(shorter.begin(), shorter.end(), [](const std::string& token) {
    return std::any_of(token.begin(), token.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
  });
  if (!has_model_number) {
    return false;
  }

  distance = longer.size() - shorter.size();
  return true;
}

inline std::string normalize_vendor(const std::string& vendor, const std::string& model = {}) {
  const std::string combined = ascii_lower(vendor + " " + model);
  if (combined.find("nvidia") != std::string::npos || combined.find("geforce") != std::string::npos ||
      combined.find("quadro") != std::string::npos || combined.find("tesla") != std::string::npos) {
    return "nvidia";
  }
  if (combined.find("advanced micro devices") != std::string::npos || combined.find("amd") != std::string::npos ||
      combined.find("radeon") != std::string::npos || combined.find("firepro") != std::string::npos ||
      combined.find("firegl") != std::string::npos) {
    return "amd";
  }
  if (combined.find("intel") != std::string::npos || combined.find("iris") != std::string::npos ||
      combined.find("uhd graphics") != std::string::npos || combined.find("hd graphics") != std::string::npos ||
      combined.find("arc ") != std::string::npos) {
    return "intel";
  }
  return trim(ascii_lower(vendor));
}

inline std::string normalize_pci_id(std::string value) {
  value = ascii_lower(trim(std::move(value)));
  if (value.rfind("0x", 0) == 0) {
    value.erase(0, 2);
  }
  return value;
}

inline std::vector<std::string> parse_csv_row(const std::string& line, bool& valid) {
  std::vector<std::string> values;
  std::string value;
  bool quoted = false;
  valid = true;
  for (std::size_t i = 0; i < line.size(); ++i) {
    const char c = line[i];
    if (c == '"') {
      if (quoted && i + 1 < line.size() && line[i + 1] == '"') {
        value.push_back('"');
        ++i;
      } else {
        quoted = !quoted;
      }
    } else if (c == ',' && !quoted) {
      values.push_back(value);
      value.clear();
    } else {
      value.push_back(c);
    }
  }
  valid = !quoted;
  values.push_back(value);
  return values;
}

inline std::vector<std::string> split_aliases(const std::string& value) {
  std::vector<std::string> aliases;
  std::size_t start = 0;
  while (start <= value.size()) {
    const auto end = value.find('|', start);
    const auto alias = trim(value.substr(start, end == std::string::npos ? std::string::npos : end - start));
    if (!alias.empty()) {
      aliases.push_back(alias);
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  return aliases;
}

inline bool parse_unsigned(std::string value, std::uint64_t& result) {
  value.erase(std::remove(value.begin(), value.end(), ','), value.end());
  value = trim(std::move(value));
  if (value.empty()) {
    result = 0;
    return true;
  }
  try {
    std::size_t consumed = 0;
    const auto parsed = std::stoull(value, &consumed);
    if (consumed != value.size()) {
      return false;
    }
    result = parsed;
    return true;
  } catch (...) {
    return false;
  }
}

inline void add_unique(std::vector<std::size_t>& indices, std::size_t index) {
  if (std::find(indices.begin(), indices.end(), index) == indices.end()) {
    indices.push_back(index);
  }
}

}  // namespace detail

inline std::string normalizeGpuModel(const std::string& model) { return detail::normalize_model(model, false); }

class GpuCatalog {
 public:
  HWI_NODISCARD const std::vector<GpuCatalogEntry>& entries() const { return entries_; }
  HWI_NODISCARD const std::string& source() const { return source_; }
  HWI_NODISCARD const std::string& retrieved_at() const { return retrieved_at_; }
  HWI_NODISCARD bool empty() const { return entries_.empty(); }

  void clear() {
    entries_.clear();
    source_.clear();
    retrieved_at_.clear();
  }

  bool addEntry(GpuCatalogEntry entry, std::string* error = nullptr) {
    if (error != nullptr) {
      error->clear();
    }
    entry.canonical_model = detail::trim(std::move(entry.canonical_model));
    entry.vendor = detail::normalize_vendor(entry.vendor, entry.canonical_model);
    entry.vendor_id = detail::normalize_pci_id(std::move(entry.vendor_id));
    entry.device_id = detail::normalize_pci_id(std::move(entry.device_id));
    if (entry.canonical_model.empty() || entry.score == 0) {
      if (error != nullptr) {
        *error = "GPU catalog entries require a non-empty canonical_model and a positive score";
      }
      return false;
    }
    entries_.push_back(std::move(entry));
    return true;
  }

  bool loadCsv(std::istream& stream, std::string* error = nullptr) {
    if (error != nullptr) {
      error->clear();
    }
    GpuCatalog loaded;
    std::map<std::string, std::size_t> columns;
    std::string line;
    std::size_t line_number = 0;
    bool have_header = false;

    auto fail = [&](const std::string& message) {
      if (error != nullptr) {
        *error = "line " + std::to_string(line_number) + ": " + message;
      }
      return false;
    };

    while (std::getline(stream, line)) {
      ++line_number;
      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }
      if (line_number == 1 && line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xef &&
          static_cast<unsigned char>(line[1]) == 0xbb && static_cast<unsigned char>(line[2]) == 0xbf) {
        line.erase(0, 3);
      }
      const auto trimmed = detail::trim(line);
      if (trimmed.empty()) {
        continue;
      }
      if (trimmed[0] == '#') {
        const auto separator = trimmed.find('=');
        if (separator != std::string::npos) {
          const auto key = detail::ascii_lower(detail::trim(trimmed.substr(1, separator - 1)));
          const auto value = detail::trim(trimmed.substr(separator + 1));
          if (key == "source") {
            loaded.source_ = value;
          } else if (key == "retrieved_at") {
            loaded.retrieved_at_ = value;
          }
        }
        continue;
      }

      bool valid = false;
      auto fields = detail::parse_csv_row(line, valid);
      if (!valid) {
        return fail("unterminated quoted CSV field");
      }

      if (!have_header) {
        for (std::size_t i = 0; i < fields.size(); ++i) {
          columns[detail::ascii_lower(detail::trim(fields[i]))] = i;
        }
        if (columns.count("canonical_model") == 0 || columns.count("score") == 0) {
          return fail("CSV header must contain canonical_model and score columns");
        }
        have_header = true;
        continue;
      }

      auto field = [&](const std::string& name) -> std::string {
        const auto column = columns.find(name);
        if (column == columns.end() || column->second >= fields.size()) {
          return {};
        }
        return detail::trim(fields[column->second]);
      };

      GpuCatalogEntry entry;
      entry.canonical_model = field("canonical_model");
      entry.vendor = field("vendor");
      entry.aliases = detail::split_aliases(field("aliases"));
      entry.vendor_id = field("vendor_id");
      entry.device_id = field("device_id");

      if (!detail::parse_unsigned(field("score"), entry.score) || entry.score == 0) {
        return fail("invalid positive score for " + entry.canonical_model);
      }
      std::uint64_t rank = 0;
      if (!detail::parse_unsigned(field("rank"), rank) || rank > std::numeric_limits<std::uint32_t>::max()) {
        return fail("invalid rank for " + entry.canonical_model);
      }
      entry.rank = static_cast<std::uint32_t>(rank);

      std::string entry_error;
      if (!loaded.addEntry(std::move(entry), &entry_error)) {
        return fail(entry_error);
      }
    }

    if (!have_header) {
      return fail("CSV contains no header");
    }
    if (loaded.entries_.empty()) {
      return fail("CSV contains no GPU entries");
    }
    *this = std::move(loaded);
    return true;
  }

  bool loadCsvFile(const std::string& path, std::string* error = nullptr) {
    if (error != nullptr) {
      error->clear();
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
      if (error != nullptr) {
        *error = "unable to open GPU catalog: " + path;
      }
      return false;
    }
    return loadCsv(stream, error);
  }

  HWI_NODISCARD GpuLookupResult lookup(const std::string& model, const std::string& vendor = {},
                                       const std::string& vendor_id = {}, const std::string& device_id = {}) const {
    const auto strict_key = detail::normalize_model(model, false);
    const auto relaxed_key = detail::normalize_model(model, true);
    const auto normalized_vendor = detail::normalize_vendor(vendor, model);
    const auto normalized_vendor_id = detail::normalize_pci_id(vendor_id);
    const auto normalized_device_id = detail::normalize_pci_id(device_id);

    std::vector<std::size_t> pci_matches;
    std::vector<std::size_t> strict_matches;
    std::vector<std::size_t> relaxed_matches;
    std::vector<std::size_t> fuzzy_matches;
    std::size_t fuzzy_distance = std::numeric_limits<std::size_t>::max();

    for (std::size_t i = 0; i < entries_.size(); ++i) {
      const auto& entry = entries_[i];
      if (!normalized_vendor.empty() && !entry.vendor.empty() && normalized_vendor != entry.vendor) {
        continue;
      }

      if (!normalized_vendor_id.empty() && !normalized_device_id.empty() && !entry.vendor_id.empty() &&
          !entry.device_id.empty() && normalized_vendor_id == entry.vendor_id && normalized_device_id == entry.device_id) {
        detail::add_unique(pci_matches, i);
      }

      auto consider_name = [&](const std::string& candidate) {
        const auto candidate_strict_key = detail::normalize_model(candidate, false);
        if (!strict_key.empty() && candidate_strict_key == strict_key) {
          detail::add_unique(strict_matches, i);
        }
        if (!relaxed_key.empty() && detail::normalize_model(candidate, true) == relaxed_key) {
          detail::add_unique(relaxed_matches, i);
        }
        std::size_t distance = 0;
        if (!strict_key.empty() && detail::fuzzy_model_prefix_distance(strict_key, candidate_strict_key, distance)) {
          if (distance < fuzzy_distance) {
            fuzzy_distance = distance;
            fuzzy_matches.clear();
          }
          if (distance == fuzzy_distance) {
            detail::add_unique(fuzzy_matches, i);
          }
        }
      };
      consider_name(entry.canonical_model);
      for (const auto& alias : entry.aliases) {
        consider_name(alias);
      }
    }

    const bool using_fuzzy_match = pci_matches.empty() && strict_matches.empty() && relaxed_matches.empty();
    const auto& matches = !pci_matches.empty() ? pci_matches
                          : !strict_matches.empty() ? strict_matches
                          : !relaxed_matches.empty() ? relaxed_matches
                                                     : fuzzy_matches;
    GpuLookupResult result;
    if (matches.empty()) {
      return result;
    }
    if (matches.size() == 1) {
      result.status = GpuLookupStatus::MATCHED;
      result.entry = entries_[matches.front()];
      return result;
    }
    if (using_fuzzy_match) {
      // Driver names often omit qualifiers such as VRAM size. When several
      // equally close catalog variants remain, use the lowest score so an
      // uncertain fuzzy match cannot overstate the detected GPU's capability.
      const auto weakest = std::min_element(matches.begin(), matches.end(), [&](std::size_t left, std::size_t right) {
        if (entries_[left].score != entries_[right].score) {
          return entries_[left].score < entries_[right].score;
        }
        return entries_[left].canonical_model < entries_[right].canonical_model;
      });
      result.status = GpuLookupStatus::MATCHED;
      result.entry = entries_[*weakest];
      return result;
    }
    result.status = GpuLookupStatus::AMBIGUOUS;
    for (const auto index : matches) {
      result.candidates.push_back(entries_[index].canonical_model);
    }
    return result;
  }

 private:
  std::vector<GpuCatalogEntry> entries_;
  std::string source_;
  std::string retrieved_at_;
};

inline EvaluationStatus overall_status(EvaluationStatus cpu, EvaluationStatus gpu, EvaluationStatus memory,
                                       EvaluationStatus disk) {
  const EvaluationStatus values[] = {cpu, gpu, memory, disk};
  if (std::find(std::begin(values), std::end(values), EvaluationStatus::FAILED) != std::end(values)) {
    return EvaluationStatus::FAILED;
  }
  if (std::find(std::begin(values), std::end(values), EvaluationStatus::UNKNOWN) != std::end(values)) {
    return EvaluationStatus::UNKNOWN;
  }
  return EvaluationStatus::PASSED;
}

inline EvaluationReport evaluate(const HardwareSnapshot& snapshot, const MachineRequirements& requirement,
                                 const GpuCatalog* gpu_catalog = nullptr) {
  EvaluationReport report;

  report.cpu.required_physical_cores = requirement.min_physical_cpu_cores;
  bool cpu_incomplete = snapshot.cpus.empty();
  for (const auto& cpu : snapshot.cpus) {
    report.cpu.detected_physical_cores += cpu.physical_cores;
    report.cpu.detected_logical_cores += cpu.logical_cores;
    cpu_incomplete = cpu_incomplete || cpu.physical_cores == 0;
  }
  if (requirement.min_physical_cpu_cores == 0) {
    report.cpu.status = EvaluationStatus::PASSED;
    report.cpu.detail = "No CPU core minimum was requested";
  } else if (report.cpu.detected_physical_cores >= requirement.min_physical_cpu_cores) {
    report.cpu.status = EvaluationStatus::PASSED;
    report.cpu.detail = "The total physical core count meets the requirement";
  } else if (cpu_incomplete) {
    report.cpu.status = EvaluationStatus::UNKNOWN;
    report.cpu.detail = "One or more CPU core counts could not be detected";
  } else {
    report.cpu.status = EvaluationStatus::FAILED;
    report.cpu.detail = "The total physical core count is below the requirement";
  }

  report.memory.required_bytes = requirement.min_memory_bytes;
  report.memory.detected_bytes = snapshot.memory_bytes;
  if (requirement.min_memory_bytes == 0) {
    report.memory.status = EvaluationStatus::PASSED;
    report.memory.detail = "No memory minimum was requested";
  } else if (snapshot.memory_bytes == 0) {
    report.memory.status = EvaluationStatus::UNKNOWN;
    report.memory.detail = "Total memory could not be detected";
  } else if (snapshot.memory_bytes >= requirement.min_memory_bytes) {
    report.memory.status = EvaluationStatus::PASSED;
    report.memory.detail = "Total memory meets the requirement";
  } else {
    report.memory.status = EvaluationStatus::FAILED;
    report.memory.detail = "Total memory is below the requirement";
  }

  report.disk.solid_state_required = requirement.require_solid_state_disk;
  for (const auto& disk : snapshot.disks) {
    switch (disk.media_type) {
      case Disk::MediaType::SOLID_STATE:
        ++report.disk.solid_state_count;
        break;
      case Disk::MediaType::ROTATIONAL:
        ++report.disk.rotational_count;
        break;
      case Disk::MediaType::UNKNOWN:
        ++report.disk.unknown_count;
        break;
    }
  }
  if (!requirement.require_solid_state_disk) {
    report.disk.status = EvaluationStatus::PASSED;
    report.disk.detail = "A solid-state disk was not required";
  } else if (report.disk.solid_state_count > 0) {
    report.disk.status = EvaluationStatus::PASSED;
    report.disk.detail = "At least one solid-state disk was detected";
  } else if (snapshot.disks.empty() || report.disk.unknown_count > 0) {
    report.disk.status = EvaluationStatus::UNKNOWN;
    report.disk.detail = "No SSD was found, but one or more disk media types could not be determined";
  } else {
    report.disk.status = EvaluationStatus::FAILED;
    report.disk.detail = "All detected disks are rotational";
  }

  report.gpu.required_model = requirement.min_gpu_model;
  if (requirement.min_gpu_model.empty()) {
    report.gpu.status = EvaluationStatus::PASSED;
    report.gpu.detail = "No GPU minimum was requested";
  } else if (gpu_catalog == nullptr || gpu_catalog->empty()) {
    report.gpu.status = EvaluationStatus::UNKNOWN;
    report.gpu.detail = "A GPU catalog is required to evaluate the requested model";
  } else {
    report.gpu.catalog_source = gpu_catalog->source();
    report.gpu.catalog_retrieved_at = gpu_catalog->retrieved_at();
    const auto required_lookup = gpu_catalog->lookup(requirement.min_gpu_model);
    if (required_lookup.status == GpuLookupStatus::NOT_FOUND) {
      report.gpu.status = EvaluationStatus::UNKNOWN;
      report.gpu.detail = "The requested GPU model was not found in the catalog";
    } else if (required_lookup.status == GpuLookupStatus::AMBIGUOUS) {
      report.gpu.status = EvaluationStatus::UNKNOWN;
      report.gpu.candidates = required_lookup.candidates;
      report.gpu.detail = "The requested GPU model matches multiple catalog entries";
    } else {
      report.gpu.required_canonical_model = required_lookup.entry.canonical_model;
      report.gpu.required_score = required_lookup.entry.score;
      bool unresolved_detected_gpu = false;

      for (const auto& gpu : snapshot.gpus) {
        const auto lookup = gpu_catalog->lookup(gpu.model, gpu.vendor, gpu.vendor_id, gpu.device_id);
        if (lookup.status != GpuLookupStatus::MATCHED) {
          unresolved_detected_gpu = true;
          report.gpu.unresolved_models.push_back(gpu.model);
          if (lookup.status == GpuLookupStatus::AMBIGUOUS) {
            report.gpu.candidates.insert(report.gpu.candidates.end(), lookup.candidates.begin(), lookup.candidates.end());
          }
          continue;
        }
        if (lookup.entry.score > report.gpu.detected_score) {
          report.gpu.detected_model = gpu.model;
          report.gpu.detected_canonical_model = lookup.entry.canonical_model;
          report.gpu.detected_score = lookup.entry.score;
        }
      }

      if (report.gpu.detected_score >= report.gpu.required_score) {
        report.gpu.status = EvaluationStatus::PASSED;
        report.gpu.detail = "At least one detected GPU meets the catalog score requirement";
      } else if (unresolved_detected_gpu && requirement.allow_unrecognized_gpu) {
        report.gpu.status = EvaluationStatus::PASSED;
        report.gpu.passed_by_unrecognized_policy = true;
        report.gpu.detail = "At least one detected GPU is unrecognized and the requirement allows it to pass";
      } else if (snapshot.gpus.empty()) {
        report.gpu.status = EvaluationStatus::UNKNOWN;
        report.gpu.detail = "No GPU was detected";
      } else if (unresolved_detected_gpu) {
        report.gpu.status = EvaluationStatus::UNKNOWN;
        report.gpu.detail = "No known GPU meets the requirement, and at least one detected GPU could not be resolved";
      } else {
        report.gpu.status = EvaluationStatus::FAILED;
        report.gpu.detail = "All detected GPUs are below the catalog score requirement";
      }
    }
  }

  report.overall = overall_status(report.cpu.status, report.gpu.status, report.memory.status, report.disk.status);
  return report;
}

inline EvaluationReport evaluate(const HardwareSnapshot& snapshot, const MachineRequirements& requirement,
                                 const GpuCatalog& gpu_catalog) {
  return evaluate(snapshot, requirement, &gpu_catalog);
}

inline EvaluationReport evaluateCurrentMachine(const MachineRequirements& requirement,
                                               const GpuCatalog* gpu_catalog = nullptr) {
  return evaluate(collectHardwareSnapshot(), requirement, gpu_catalog);
}

inline EvaluationReport evaluateCurrentMachine(const MachineRequirements& requirement, const GpuCatalog& gpu_catalog) {
  return evaluateCurrentMachine(requirement, &gpu_catalog);
}

}  // namespace hwinfo::requirements
