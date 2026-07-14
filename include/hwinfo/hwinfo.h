// Copyright Leon Freist
// Header-only hardware information library.

#pragma once

#if defined(unix) || defined(__unix) || defined(__unix__)
#define HWINFO_UNIX
#endif
#if defined(__APPLE__)
#define HWINFO_APPLE
#endif
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#define HWINFO_WINDOWS
#define NOMINMAX
#endif

#if defined(__x86_64__) || defined(__x86_64) || defined(__amd64__) || defined(_M_X64)
#define HWINFO_X86_64
#elif defined(__i386__) || defined(_M_IX86)
#define HWINFO_X86_32
#endif
#if defined(HWINFO_X86_64) || defined(HWINFO_X86_32)
#define HWINFO_X86
#endif

#if defined(__arm__) || defined(_M_ARM)
#define HWINFO_ARM32
#elif defined(__aarch64__) || defined(_M_ARM64)
#define HWINFO_AARCH64
#endif

#if defined(HWINFO_ARM32) || defined(HWINFO_AARCH64)
#define HWINFO_ARM
#endif

// Kept for source compatibility. A header-only library does not import or
// export symbols from a binary.
#define HWINFO_API

#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(nodiscard)
#define HWI_NODISCARD [[nodiscard]]
#endif
#endif

#ifndef HWI_NODISCARD
#define HWI_NODISCARD
#endif

// Common dependencies
#include <clocale>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <limits>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#ifdef HWINFO_WINDOWS
#include <WbemIdl.h>
#include <comdef.h>
#include <devguid.h>
#include <dxgi1_6.h>
#include <intrin.h>
#include <setupapi.h>
#include <windows.h>
#include <winioctl.h>

#include <algorithm>
#include <cctype>
#include <map>
#include <optional>
#endif

#ifdef HWINFO_APPLE
#include <Availability.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOMedia.h>
#include <mach/mach_host.h>
#include <mach/mach_init.h>
#include <mach/vm_statistics.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <regex>
#include <unordered_map>
#endif

#if defined(HWINFO_UNIX) && !defined(HWINFO_APPLE)
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#endif

// Utility implementation
//
// Created by leon- on 23/06/2023.
//

namespace hwinfo::utils {

template <typename T>
inline bool is_power_of_two(T x) {
  return x > 0 && (x & (x - 1)) == 0;
}

template <typename T>
inline T round_to_next_power_of_2(T val) {
  if (val == 0) return 0;
  if (is_power_of_two(val)) return val;
  val |= val >> 1;
  val |= val >> 2;
  val |= val >> 4;
  val |= val >> 8;
  val |= val >> 16;
  return val + 1;
}

}  // namespace hwinfo::utils

namespace hwinfo::utils {

/**
 * remove all white spaces (' ', '\t', '\n') from start and end of input
 * inplace!
 * @param input
 */
inline void strip(std::string& input) {
  auto start = input.find_first_not_of(" \t\r\n");
  auto end = input.find_last_not_of(" \t\r\n");
  if (start == std::string::npos) {
    input.clear();
  } else {
    input = input.substr(start, end - start + 1);
  }
}

/**
 * Split input string at delimiter and return result
 * @param input
 * @param delimiter
 * @return
 */
inline std::vector<std::string> split(const std::string& input, const std::string& delimiter) {
  std::vector<std::string> result;
  size_t shift = 0;
  while (true) {
    size_t match = input.find(delimiter, shift);
    result.emplace_back(input.substr(shift, match - shift));
    if (match == std::string::npos) {
      break;
    }
    shift = match + delimiter.size();
  }
  return result;
}

/**
 * Split input string at delimiter (char) and return result
 * @param input
 * @param delimiter
 * @return
 */
inline std::vector<std::string> split(const std::string& input, const char delimiter) {
  std::vector<std::string> result;
  size_t shift = 0;
  while (true) {
    size_t match = input.find(delimiter, shift);
    if (match == std::string::npos) {
      break;
    }
    result.emplace_back(input.substr(shift, match - shift));
    shift = match + 1;
  }
  return result;
}

/**
 * Convert wstring to string
 * @return
 */
inline std::string wstring_to_std_string(const std::wstring& ws) {
  std::string str_locale = std::setlocale(LC_ALL, NULL);

#if _MSC_VER
  std::setlocale(LC_ALL, ".65001");
#else
  std::setlocale(LC_ALL, ".UTF-8");
#endif
  const wchar_t* wch_src = ws.c_str();

#ifdef _MSC_VER
  size_t n_dest_size;
  wcstombs_s(&n_dest_size, nullptr, 0, wch_src, 0);
  n_dest_size++;  // Increase by one for null terminator

  char* ch_dest = new char[n_dest_size];
  memset(ch_dest, 0, n_dest_size);

  size_t n_convert_size;
  wcstombs_s(&n_convert_size, ch_dest, n_dest_size, wch_src,
             n_dest_size - 1);  // subtract one to ignore null terminator

  std::string result_text = ch_dest;
  delete[] ch_dest;
#else
  size_t n_dest_size = std::wcstombs(NULL, wch_src, 0) + 1;
  char* ch_dest = new char[n_dest_size];
  std::memset(ch_dest, 0, n_dest_size);
  std::wcstombs(ch_dest, wch_src, n_dest_size);
  std::string result_text = ch_dest;
  delete[] ch_dest;
#endif

  std::setlocale(LC_ALL, str_locale.c_str());
  return result_text;
}

/**
 * Replace the std::string::starts_with function only available in C++20 and above.
 * @param str
 * @param prefix
 * @return
 */
template <typename string_type, typename prefix_type>
inline bool starts_with(const string_type& str, const prefix_type& prefix) {
#ifdef __cpp_lib_starts_ends_with
  return str.starts_with(prefix);
#else
  return str.rfind(prefix, 0) == 0;
#endif
}

inline std::string join(const std::vector<std::string>& values, const std::string& delim) {
  if (values.empty()) {
    return {};
  }
  if (values.size() == 1) {
    return values[0];
  }
  std::stringstream ss;
  for (std::size_t i = 0; i < values.size() - 1; ++i) {
    ss << values[i] << delim;
  }
  ss << values.back();
  return ss.str();
}

}  // namespace hwinfo::utils

namespace hwinfo::unit {

enum class SiPrefix : std::uint64_t {
  KILO = 1000ull,
  MEGA = 1000'000ull,
  GIGA = 1000'000'000ull,
  TERA = 1000'000'000'000ull
};

template <typename T>
inline auto operator*(T val, SiPrefix prefix) {
  if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>) {
    return static_cast<double>(val) * static_cast<double>(prefix);
  } else if constexpr (std::is_unsigned_v<T>) {
    return static_cast<std::uint64_t>(val) * static_cast<std::uint64_t>(prefix);
  } else {
    return static_cast<std::int64_t>(val) * static_cast<std::int64_t>(prefix);
  }
}

enum class IECPrefix : std::uint64_t {
  KIBI = 1024ull,
  MEBI = 1024ull * 1024ull,
  GIBI = 1024ull * 1024ull * 1024ull,
  TEBI = 1024ull * 1024ull * 1024ull * 1024ull
};

template <typename T>
inline auto operator*(T val, IECPrefix prefix) {
  if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>) {
    return static_cast<double>(val) * static_cast<double>(prefix);
  } else if constexpr (std::is_unsigned_v<T>) {
    return static_cast<std::uint64_t>(val) * static_cast<std::uint64_t>(prefix);
  } else {
    return static_cast<std::int64_t>(val) * static_cast<std::int64_t>(prefix);
  }
}

/**
 * @brief Convert bytes to MiB/MB
 *
 * @param bytes number of bytes
 * @return number of MiB as double
 */
inline double unit_prefix_to(std::uint64_t value, IECPrefix prefix) {
  if (utils::is_power_of_two(value)) {
    auto res = static_cast<double>(value / static_cast<std::uint64_t>(prefix));
    if (res != 0) {
      return res;
    }
  }
  return static_cast<double>(value) / static_cast<double>(prefix);
}

inline double unit_prefix_to(std::uint64_t value, SiPrefix prefix) {
  if (utils::is_power_of_two(value)) {
    return static_cast<double>(value / static_cast<std::uint64_t>(prefix));
  }
  return static_cast<double>(value) / static_cast<double>(prefix);
}

}  // namespace hwinfo::unit

#ifdef HWINFO_APPLE

namespace hwinfo::utils {

// Get a string value from sysctl
inline std::string getSysctlString(const char* name, std::string defaultValue = "", size_t initialSize = 256) {
  auto buffer = std::string(initialSize, '\0');
  size_t size = buffer.size();
  if (sysctlbyname(name, buffer.data(), &size, nullptr, 0) == 0) {
    buffer.resize(size);
    return buffer;
  }
  return defaultValue;
}

// Get an integer value from sysctl
template <typename T>
T getSysctlValue(const char* name, T defaultValue = T()) {
  T value;
  size_t size = sizeof(value);
  if (sysctlbyname(name, &value, &size, nullptr, 0) == 0) {
    return value;
  }
  return defaultValue;
}

}  // namespace hwinfo::utils

#endif  // HWINFO_APPLE

#ifdef HWINFO_WINDOWS

namespace hwinfo::internal::utils {

template <typename T>
T getRegistryValue(HKEY hKeyParent, const std::wstring& subkey, const std::wstring& valueName) {
  HKEY hKey;
  if (RegOpenKeyExW(hKeyParent, subkey.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
    return T{};
  }

  T result{};
  DWORD dwType = 0;
  DWORD dwSize = 0;

  if (RegQueryValueExW(hKey, valueName.c_str(), nullptr, &dwType, nullptr, &dwSize) == ERROR_SUCCESS) {
    if constexpr (std::is_same_v<T, std::string>) {
      std::wstring wbuffer(dwSize / sizeof(wchar_t), L'\0');
      if (RegQueryValueExW(hKey, valueName.c_str(), nullptr, nullptr, (LPBYTE)wbuffer.data(), &dwSize) ==
          ERROR_SUCCESS) {
        if (!wbuffer.empty() && wbuffer.back() == L'\0') wbuffer.pop_back();
        std::wstring tmp(wbuffer.begin(), wbuffer.end());
        result = hwinfo::utils::wstring_to_std_string(tmp);
      }
    } else if constexpr (std::is_integral_v<T>) {
      DWORD dwResult = 0;
      if (RegQueryValueExW(hKey, valueName.c_str(), nullptr, nullptr, (LPBYTE)&dwResult, &dwSize) == ERROR_SUCCESS) {
        result = static_cast<T>(dwResult);
      }
    }
  }

  RegCloseKey(hKey);
  return result;
}

}  // namespace hwinfo::internal::utils

#endif

#ifdef HWINFO_WINDOWS

#ifndef __MINGW32__
#pragma comment(lib, "wbemuuid.lib")
#endif

namespace hwinfo {

namespace utils {
namespace WMI {

struct _WMI {
  _WMI();
  ~_WMI();
  bool execute_query(const std::wstring& query);

  IWbemLocator* locator = nullptr;
  IWbemServices* service = nullptr;
  IEnumWbemClassObject* enumerator = nullptr;
};

template <typename T>
std::vector<T> query(const std::wstring& wmi_class, const std::wstring& field, const std::wstring& filter = L"");

}  // namespace WMI
}  // namespace utils
}  // namespace hwinfo

#endif

#if defined(HWINFO_UNIX) && !defined(HWINFO_APPLE)

namespace hwinfo::unix_os::cpu {

struct CPU {
  struct Core {
    std::uint64_t id = 0;
    std::array<std::uint64_t, 4> cache_bytes = {0, 0, 0, 0};
    std::uint64_t regular_frequency_hz = 0;
    std::uint64_t max_frequency_hz = 0;
    bool smt = false;
    std::vector<std::string> flags;
  };
  std::uint32_t socket_id;
  std::string model;
  std::string vendor;
  std::map<std::uint32_t, Core> cores;
};

/**
 * parses the provided /proc/cpuinfo file conenten into a map <socket_id, <core_id, <key, value>>>
 */
std::map<std::uint32_t, std::map<std::uint32_t, std::map<std::string, std::string>>> parse_cpuinfo_file(
    const std::string& data);

/**
 * parses a sockets map (generated by e.g. parse_cpuinfo_file) into a vector of CPU instances
 */
std::map<std::uint32_t, unix_os::cpu::CPU> parse_cpus(
    const std::map<std::uint32_t, std::map<std::uint32_t, std::map<std::string, std::string>>>& sockets);

}  // namespace hwinfo::unix_os::cpu
#endif

// Public API

namespace hwinfo {

class HWINFO_API CPU {
  friend HWINFO_API std::vector<CPU> getAllCPUs();

 public:
  static constexpr std::uint32_t invalid_id = std::numeric_limits<std::uint32_t>::max();

 public:
  struct Cache {
    std::uint64_t l1_data;
    std::uint64_t l1_instruction;
    std::uint64_t l2;
    std::uint64_t l3;
  };
  struct Core {
    std::uint64_t id;
    Cache cache;
    std::uint64_t regular_frequency_hz;
    std::uint64_t max_frequency_hz;
    bool smt;
  };

 public:
  ~CPU() = default;

  HWI_NODISCARD std::uint32_t id() const;
  HWI_NODISCARD const std::string& modelName() const;
  HWI_NODISCARD const std::string& vendor() const;
  HWI_NODISCARD std::uint64_t numPhysicalCores() const;
  HWI_NODISCARD std::uint64_t numLogicalCores() const;
  HWI_NODISCARD const std::vector<std::string>& flags() const;
  HWI_NODISCARD const std::vector<Core>& cores() const;

 private:
  CPU() = default;

  std::uint32_t _id = invalid_id;
  std::string _modelName;
  std::string _vendor;
  std::uint64_t _numPhysicalCores = 0;
  std::uint64_t _numLogicalCores = 0;
  std::vector<std::string> _flags;
  std::vector<Core> _cores;
};

HWINFO_API std::vector<CPU> getAllCPUs();

}  // namespace hwinfo

namespace hwinfo {

class HWINFO_API Disk {
 public:
  enum class Interface {
    NVME,
    USB,
    USB1,
    USB2,
    USB3_5GBit,
    USB3_10GBit,
    USB3_20GBit,
    USB4_20GBit,
    USB4_40GBit,
    USB4_80GBit,
    SATA,
    SCSI,
    UNKNOWN
  };

  enum class MediaType {
    SOLID_STATE,
    ROTATIONAL,
    UNKNOWN
  };

  friend HWINFO_API std::vector<Disk> getAllDisks();
  friend HWINFO_API std::ostream& operator<<(std::ostream& os, const Disk::Interface& disk_interface);

 public:
  static constexpr std::uint32_t invalid_id = std::numeric_limits<std::uint32_t>::max();

 public:
  ~Disk() = default;

  HWI_NODISCARD const std::string& vendor() const;
  HWI_NODISCARD const std::string& model() const;
  HWI_NODISCARD const std::string& serial_number() const;
  HWI_NODISCARD std::uint64_t size() const;
  HWI_NODISCARD const std::vector<std::string>& mount_points() const;
  HWI_NODISCARD std::uint32_t id() const;
  HWI_NODISCARD Interface disk_interface() const;
  HWI_NODISCARD MediaType media_type() const;
  /// Kept for source compatibility. Prefer media_type() when an unknown result must be distinguished from an HDD.
  HWI_NODISCARD bool is_solid_state() const;

 private:
  Disk() = default;

  std::string _vendor = "<unknown>";
  std::string _model = "<unknown>";
  std::string _serial_number = "<unknown>";
  std::uint64_t _size_bytes = 0;
  std::vector<std::string> _mount_points;
  std::uint32_t _id = invalid_id;
  Interface _interface = Interface::UNKNOWN;
  MediaType _media_type = MediaType::UNKNOWN;
};

HWINFO_API std::vector<Disk> getAllDisks();

HWINFO_API std::ostream& operator<<(std::ostream& os, const hwinfo::Disk::Interface& disk_interface);

}  // namespace hwinfo

namespace hwinfo {

class HWINFO_API GPU {
  friend HWINFO_API std::vector<GPU> getAllGPUs();

 public:
  static constexpr std::uint32_t invalid_id = std::numeric_limits<std::uint32_t>::max();

 public:
  ~GPU() = default;

  HWI_NODISCARD const std::string& vendor() const;
  HWI_NODISCARD const std::string& name() const;
  HWI_NODISCARD uint64_t dedicated_memory_Bytes() const;
  HWI_NODISCARD uint64_t shared_memory_Bytes() const;
  HWI_NODISCARD uint64_t frequency_hz() const;
  HWI_NODISCARD std::uint32_t id() const;
  HWI_NODISCARD const std::string& vendor_id() const;
  HWI_NODISCARD const std::string& device_id() const;

 private:
  GPU() = default;

 private:
  std::string _vendor;
  std::string _name;
  uint64_t _dedicated_memory_Bytes = 0;
  uint64_t _shared_memory_Bytes = 0;
  uint64_t _frequency_hz = 0;
  std::uint32_t _id = invalid_id;
  std::string _vendor_id;
  std::string _device_id;
};

HWINFO_API std::vector<GPU> getAllGPUs();

}  // namespace hwinfo

namespace hwinfo {

class HWINFO_API MainBoard {
  friend std::string get_dmi_by_name(const std::string& name);

 public:
  MainBoard();
  ~MainBoard() = default;

  HWI_NODISCARD const std::string& vendor() const;
  HWI_NODISCARD const std::string& name() const;
  HWI_NODISCARD const std::string& version() const;
  HWI_NODISCARD const std::string& serialNumber() const;

 private:
  std::string _vendor;
  std::string _name;
  std::string _version;
  std::string _serial_number;
};

}  // namespace hwinfo

namespace hwinfo {

class HWINFO_API Network {
  friend HWINFO_API std::vector<Network> getAllNetworks();

 public:
  ~Network() = default;

  HWI_NODISCARD const std::string& interfaceIndex() const;
  HWI_NODISCARD const std::string& description() const;
  HWI_NODISCARD const std::string& mac() const;
  HWI_NODISCARD const std::string& ip4() const;
  HWI_NODISCARD const std::string& ip6() const;

 private:
  Network() = default;

  std::string _index;
  std::string _description;
  std::string _mac;
  std::string _ip4;
  std::string _ip6;
};

HWINFO_API std::vector<Network> getAllNetworks();

}  // namespace hwinfo

namespace hwinfo {

class HWINFO_API OS {
 public:
  OS();
  ~OS() = default;

  HWI_NODISCARD std::string name() const;
  HWI_NODISCARD std::string version() const;
  HWI_NODISCARD std::string kernel() const;
  HWI_NODISCARD bool is32bit() const;
  HWI_NODISCARD bool is64bit() const;
  HWI_NODISCARD bool isBigEndian() const;
  HWI_NODISCARD bool isLittleEndian() const;

 private:
  std::string _name;
  std::string _version;
  std::string _kernel;
  bool _32bit = false;
  bool _64bit = false;
  bool _bigEndian = false;
  bool _littleEndian = false;
};

}  // namespace hwinfo

namespace hwinfo {

class HWINFO_API Memory {
 public:
  static constexpr std::uint32_t invalid_id = std::numeric_limits<std::uint32_t>::max();

 public:
  struct Module {
    std::uint32_t id = invalid_id;
    std::string vendor;
    std::string name;
    std::string model;
    std::string serial_number;
    uint64_t _size_bytes = 0;
    uint64_t frequency_hz = 0;
  };

 public:
  Memory();
  ~Memory() = default;

  HWI_NODISCARD const std::vector<Memory::Module>& modules() const;
  HWI_NODISCARD uint64_t size() const;
  HWI_NODISCARD uint64_t free() const;
  HWI_NODISCARD uint64_t available() const;

 private:
  std::vector<Memory::Module> _modules;
};

}  // namespace hwinfo

// Common implementation

namespace hwinfo {

inline std::uint32_t CPU::id() const { return _id; }

inline const std::string& CPU::modelName() const { return _modelName; }

inline const std::string& CPU::vendor() const { return _vendor; }

inline std::uint64_t CPU::numPhysicalCores() const { return _numPhysicalCores; }

inline std::uint64_t CPU::numLogicalCores() const { return _numLogicalCores; }

inline const std::vector<std::string>& CPU::flags() const { return _flags; }

inline const std::vector<CPU::Core>& CPU::cores() const { return _cores; }

}  // namespace hwinfo

namespace hwinfo {

inline const std::string& Disk::vendor() const { return _vendor; }

inline const std::string& Disk::model() const { return _model; }

inline const std::string& Disk::serial_number() const { return _serial_number; }

inline uint64_t Disk::size() const { return _size_bytes; }

inline std::uint32_t Disk::id() const { return _id; }

inline const std::vector<std::string>& Disk::mount_points() const { return _mount_points; }

inline Disk::Interface Disk::disk_interface() const { return _interface; }

inline Disk::MediaType Disk::media_type() const { return _media_type; }

inline bool Disk::is_solid_state() const { return _media_type == MediaType::SOLID_STATE; }

inline std::ostream& operator<<(std::ostream& os, const Disk::Interface& disk_interface) {
  switch (disk_interface) {
    case Disk::Interface::NVME:
      os << "Disk::Interface::NVME";
      break;
    case hwinfo::Disk::Interface::USB:
      os << "Disk::Interface::USB";
      break;
    case Disk::Interface::USB1:
      os << "Disk::Interface::USB1";
      break;
    case Disk::Interface::USB2:
      os << "Disk::Interface::USB2";
      break;
    case Disk::Interface::USB3_5GBit:
      os << "Disk::Interface::USB3_5GBit";
      break;
    case Disk::Interface::USB3_10GBit:
      os << "Disk::Interface::USB3_10GBit";
      break;
    case Disk::Interface::USB3_20GBit:
      os << "Disk::Interface::USB3_20GBit";
      break;
    case Disk::Interface::USB4_20GBit:
      os << "Disk::Interface::USB4_20GBit";
      break;
    case Disk::Interface::USB4_40GBit:
      os << "Disk::Interface::USB4_40GBit";
      break;
    case Disk::Interface::USB4_80GBit:
      os << "Disk::Interface::USB3_80GBit";
      break;
    case Disk::Interface::SATA:
      os << "Disk::Interface::SATA";
      break;
    case Disk::Interface::SCSI:
      os << "Disk::Interface::SCSI";
      break;
    case Disk::Interface::UNKNOWN:
      os << "Disk::Interface::UNKNOWN";
      break;
  }
  return os;
}

}  // namespace hwinfo

namespace hwinfo {

inline const std::string& GPU::vendor() const { return _vendor; }

inline const std::string& GPU::name() const { return _name; }

inline uint32_t GPU::id() const { return _id; }

inline uint64_t GPU::dedicated_memory_Bytes() const { return _dedicated_memory_Bytes; }

inline uint64_t GPU::shared_memory_Bytes() const { return _shared_memory_Bytes; }

inline uint64_t GPU::frequency_hz() const { return _frequency_hz; }

inline const std::string& GPU::vendor_id() const { return _vendor_id; }

inline const std::string& GPU::device_id() const { return _device_id; }

}  // namespace hwinfo

namespace hwinfo {

inline const std::string& MainBoard::vendor() const { return _vendor; }

inline const std::string& MainBoard::name() const { return _name; }

inline const std::string& MainBoard::version() const { return _version; }

inline const std::string& MainBoard::serialNumber() const { return _serial_number; }

}  // namespace hwinfo

namespace hwinfo {

inline const std::string& Network::interfaceIndex() const { return _index; }

inline const std::string& Network::description() const { return _description; }

inline const std::string& Network::mac() const { return _mac; }

inline const std::string& Network::ip4() const { return _ip4; }

inline const std::string& Network::ip6() const { return _ip6; }

}  // namespace hwinfo

namespace hwinfo {

inline std::string OS::name() const { return _name; }

inline std::string OS::version() const { return _version; }

inline std::string OS::kernel() const { return _kernel; }

inline bool OS::is32bit() const { return _32bit; }

inline bool OS::is64bit() const { return _64bit; }

inline bool OS::isBigEndian() const { return _bigEndian; }

inline bool OS::isLittleEndian() const { return _littleEndian; }

}  // namespace hwinfo

namespace hwinfo {

inline const std::vector<Memory::Module>& Memory::modules() const { return _modules; }

}  // namespace hwinfo

#if defined(HWINFO_WINDOWS)
#ifdef HWINFO_WINDOWS

namespace hwinfo {
namespace utils {
namespace WMI {

inline _WMI::_WMI() {
  auto res = CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
                                  nullptr, EOAC_NONE, nullptr);
  res &= CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  res &= CoCreateInstance(__uuidof(WbemLocator), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&locator));
  if (SUCCEEDED(res)) {
    res &= locator->ConnectServer(bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &service);
    if (SUCCEEDED(res))
      res &= CoSetProxyBlanket(service, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL,
                               RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
  }
  if (FAILED(res)) {
    throw std::runtime_error("error initializing WMI");
  }
}

inline _WMI::~_WMI() {
  if (locator) locator->Release();
  if (service) service->Release();
  CoUninitialize();
}

inline bool _WMI::execute_query(const std::wstring& query) {
  if (service == nullptr) return false;
  return SUCCEEDED(service->ExecQuery(bstr_t(L"WQL"), bstr_t(std::wstring(query.begin(), query.end()).c_str()),
                                      WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumerator));
}

template <>
inline std::vector<long> query(const std::wstring& wmi_class, const std::wstring& field, const std::wstring& filter) {
  std::vector<long> result;
  _WMI wmi;
  std::wstring filter_string;
  if (!filter.empty()) {
    filter_string.append(L" WHERE " + filter);
  }
  std::wstring query_string(L"SELECT " + field + L" FROM " + wmi_class + filter_string);
  bool success = wmi.execute_query(query_string);
  if (!success) {
    return {};
  }

  ULONG u_return = 0;
  IWbemClassObject* obj = nullptr;
  while (wmi.enumerator) {
    wmi.enumerator->Next((long)WBEM_INFINITE, 1, &obj, &u_return);
    if (!u_return) {
      break;
    }
    VARIANT vt_prop;
    HRESULT hr = obj->Get(field.c_str(), 0, &vt_prop, nullptr, nullptr);
    if (SUCCEEDED(hr)) {
      result.push_back(vt_prop.intVal);
    }
    VariantClear(&vt_prop);
    obj->Release();
  }
  return result;
}

template <>
inline std::vector<int> query(const std::wstring& wmi_class, const std::wstring& field, const std::wstring& filter) {
  std::vector<int> result;
  for (const auto& v : query<long>(wmi_class, field, filter)) {
    result.push_back(static_cast<int>(v));
  }
  return result;
}

template <>
inline std::vector<bool> query(const std::wstring& wmi_class, const std::wstring& field, const std::wstring& filter) {
  std::vector<bool> result;
  _WMI wmi;
  std::wstring filter_string;
  if (!filter.empty()) {
    filter_string.append(L" WHERE " + filter);
  }
  std::wstring query_string(L"SELECT " + field + L" FROM " + wmi_class + filter_string);
  bool success = wmi.execute_query(query_string);
  if (!success) {
    return {};
  }

  ULONG u_return = 0;
  IWbemClassObject* obj = nullptr;
  while (wmi.enumerator) {
    wmi.enumerator->Next((long)WBEM_INFINITE, 1, &obj, &u_return);
    if (!u_return) {
      break;
    }
    VARIANT vt_prop;
    HRESULT hr = obj->Get(field.c_str(), 0, &vt_prop, nullptr, nullptr);
    if (SUCCEEDED(hr)) {
      result.push_back(vt_prop.boolVal);
    }
    VariantClear(&vt_prop);
    obj->Release();
  }
  return result;
}

template <>
inline std::vector<unsigned> query(const std::wstring& wmi_class, const std::wstring& field,
                                   const std::wstring& filter) {
  std::vector<unsigned> result;
  _WMI wmi;
  std::wstring filter_string;
  if (!filter.empty()) {
    filter_string.append(L" WHERE " + filter);
  }
  std::wstring query_string(L"SELECT " + field + L" FROM " + wmi_class + filter_string);
  bool success = wmi.execute_query(query_string);
  if (!success) {
    return {};
  }

  ULONG u_return = 0;
  IWbemClassObject* obj = nullptr;
  while (wmi.enumerator) {
    wmi.enumerator->Next((long)WBEM_INFINITE, 1, &obj, &u_return);
    if (!u_return) {
      break;
    }
    VARIANT vt_prop;
    HRESULT hr = obj->Get(field.c_str(), 0, &vt_prop, nullptr, nullptr);
    if (SUCCEEDED(hr)) {
      result.push_back(vt_prop.uintVal);
    }
    VariantClear(&vt_prop);
    obj->Release();
  }
  return result;
}

template <>
inline std::vector<unsigned short> query(const std::wstring& wmi_class, const std::wstring& field,
                                         const std::wstring& filter) {
  std::vector<unsigned short> result;
  _WMI wmi;
  std::wstring filter_string;
  if (!filter.empty()) {
    filter_string.append(L" WHERE " + filter);
  }
  std::wstring query_string(L"SELECT " + field + L" FROM " + wmi_class + filter_string);
  bool success = wmi.execute_query(query_string);
  if (!success) {
    return {};
  }

  ULONG u_return = 0;
  IWbemClassObject* obj = nullptr;
  while (wmi.enumerator) {
    wmi.enumerator->Next((long)WBEM_INFINITE, 1, &obj, &u_return);
    if (!u_return) {
      break;
    }
    VARIANT vt_prop;
    HRESULT hr = obj->Get(field.c_str(), 0, &vt_prop, nullptr, nullptr);
    if (SUCCEEDED(hr)) {
      result.push_back(vt_prop.uiVal);
    }
    VariantClear(&vt_prop);
    obj->Release();
  }
  return result;
}

template <>
inline std::vector<long long> query(const std::wstring& wmi_class, const std::wstring& field,
                                    const std::wstring& filter) {
  std::vector<long long> result;
  _WMI wmi;
  std::wstring filter_string;
  if (!filter.empty()) {
    filter_string.append(L" WHERE " + filter);
  }
  std::wstring query_string(L"SELECT " + field + L" FROM " + wmi_class + filter_string);
  bool success = wmi.execute_query(query_string);
  if (!success) {
    return {};
  }

  ULONG u_return = 0;
  IWbemClassObject* obj = nullptr;
  while (wmi.enumerator) {
    wmi.enumerator->Next((long)WBEM_INFINITE, 1, &obj, &u_return);
    if (!u_return) {
      break;
    }
    VARIANT vt_prop;
    HRESULT hr = obj->Get(field.c_str(), 0, &vt_prop, nullptr, nullptr);
    if (SUCCEEDED(hr)) {
      result.push_back(vt_prop.llVal);
    }
    VariantClear(&vt_prop);
    obj->Release();
  }
  return result;
}

template <>
inline std::vector<unsigned long long> query(const std::wstring& wmi_class, const std::wstring& field,
                                             const std::wstring& filter) {
  std::vector<unsigned long long> result;
  _WMI wmi;
  std::wstring filter_string;
  if (!filter.empty()) {
    filter_string.append(L" WHERE " + filter);
  }
  std::wstring query_string(L"SELECT " + field + L" FROM " + wmi_class + filter_string);
  bool success = wmi.execute_query(query_string);
  if (!success) {
    return {};
  }

  ULONG u_return = 0;
  IWbemClassObject* obj = nullptr;
  while (wmi.enumerator) {
    wmi.enumerator->Next((long)WBEM_INFINITE, 1, &obj, &u_return);
    if (!u_return) {
      break;
    }
    VARIANT vt_prop;
    HRESULT hr = obj->Get(field.c_str(), 0, &vt_prop, nullptr, nullptr);
    if (SUCCEEDED(hr)) {
      result.push_back(vt_prop.ullVal);
    }
    VariantClear(&vt_prop);
    obj->Release();
  }
  return result;
}

template <>
inline std::vector<std::string> query(const std::wstring& wmi_class, const std::wstring& field,
                                      const std::wstring& filter) {
  _WMI wmi;
  std::wstring filter_string;
  if (!filter.empty()) {
    filter_string.append(L" WHERE " + filter);
  }
  std::wstring query_string(L"SELECT " + field + L" FROM " + wmi_class + filter_string);
  bool success = wmi.execute_query(query_string);
  if (!success) {
    return {};
  }
  std::vector<std::string> result;

  ULONG u_return = 0;
  IWbemClassObject* obj = nullptr;
  while (wmi.enumerator) {
    wmi.enumerator->Next((long)WBEM_INFINITE, 1, &obj, &u_return);
    if (!u_return) {
      break;
    }
    VARIANT vt_prop;
    HRESULT hr = obj->Get(field.c_str(), 0, &vt_prop, nullptr, nullptr);
    if (SUCCEEDED(hr)) {
      result.push_back(wstring_to_std_string(vt_prop.bstrVal));
    }
    VariantClear(&vt_prop);
    obj->Release();
  }
  return result;
}

}  // namespace WMI
}  // namespace utils
}  // namespace hwinfo

#endif  // HWINFO_WINDOWS

#ifdef HWINFO_WINDOWS

inline int countSetBits(unsigned __int64 mask) {
#if defined(_M_X64) || defined(__x86_64__)
  return static_cast<int>(__popcnt64(mask));
#else
  return static_cast<int>(__popcnt(static_cast<unsigned int>(mask & 0xFFFFFFFF)) +
                          __popcnt(static_cast<unsigned int>(mask >> 32)));
#endif
}

namespace hwinfo {
inline std::vector<CPU> getAllCPUs() {
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

#ifdef HWINFO_WINDOWS

namespace {

inline hwinfo::Disk::Interface mapBusType(STORAGE_BUS_TYPE busType) {
  switch (busType) {
    case BusTypeNvme:
      return hwinfo::Disk::Interface::NVME;
    case BusTypeUsb:
      return hwinfo::Disk::Interface::USB;
    case BusTypeSata:
      return hwinfo::Disk::Interface::SATA;
    case BusTypeScsi:  // fall through
    case BusTypeSas:
      return hwinfo::Disk::Interface::SCSI;
    default:
      return hwinfo::Disk::Interface::UNKNOWN;
  }
}

inline std::map<uint32_t, std::vector<std::string>> getDiskToVolumeMap() {
  std::map<uint32_t, std::vector<std::string>> mapping;
  char logicalDrives[MAX_PATH] = {};
  GetLogicalDriveStringsA(sizeof(logicalDrives), logicalDrives);

  char* currentDrive = logicalDrives;
  while (*currentDrive) {
    std::string rootPath = currentDrive;  // e.g., "C:\"
    UINT driveType = GetDriveTypeA(rootPath.c_str());

    if (driveType == DRIVE_FIXED || driveType == DRIVE_REMOVABLE) {
      std::string volumePath = "\\\\.\\" + rootPath.substr(0, 2);
      HANDLE hVolume =
          CreateFileA(volumePath.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);

      if (hVolume != INVALID_HANDLE_VALUE) {
        STORAGE_DEVICE_NUMBER sdn;
        DWORD bytesReturned;
        if (DeviceIoControl(hVolume, IOCTL_STORAGE_GET_DEVICE_NUMBER, nullptr, 0, &sdn, sizeof(sdn), &bytesReturned,
                            nullptr)) {
          mapping[sdn.DeviceNumber].push_back(rootPath);
        }
        CloseHandle(hVolume);
      }
    }
    currentDrive += strlen(currentDrive) + 1;
  }
  return mapping;
}

inline hwinfo::Disk::MediaType getDiskMediaType(HANDLE device, const hwinfo::Disk::Interface disk_interface) {
  STORAGE_PROPERTY_QUERY query{};
  query.PropertyId = StorageDeviceSeekPenaltyProperty;
  query.QueryType = PropertyStandardQuery;

  DEVICE_SEEK_PENALTY_DESCRIPTOR descriptor{};
  DWORD bytesReturned = 0;
  if (DeviceIoControl(device, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &descriptor, sizeof(descriptor),
                      &bytesReturned, nullptr) &&
      bytesReturned >= sizeof(descriptor)) {
    return descriptor.IncursSeekPenalty == FALSE ? hwinfo::Disk::MediaType::SOLID_STATE
                                                 : hwinfo::Disk::MediaType::ROTATIONAL;
  }

  if (disk_interface == hwinfo::Disk::Interface::NVME) {
    return hwinfo::Disk::MediaType::SOLID_STATE;
  }
  return hwinfo::Disk::MediaType::UNKNOWN;
}

}  // namespace

namespace hwinfo {

inline std::vector<Disk> getAllDisks() {
  std::vector<Disk> disks;

  auto diskToVolumes = getDiskToVolumeMap();

  for (uint32_t i = 0; i < 16; ++i) {
    std::string devicePath = "\\\\.\\PhysicalDrive" + std::to_string(i);

    HANDLE hDevice =
        CreateFileA(devicePath.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);

    if (hDevice == INVALID_HANDLE_VALUE) continue;

    Disk disk;
    disk._id = i;

    STORAGE_PROPERTY_QUERY query = {StorageDeviceProperty, PropertyStandardQuery, 0};
    char buffer[1024];
    DWORD bytesReturned;
    if (DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &buffer, sizeof(buffer),
                        &bytesReturned, nullptr)) {
      auto* desc = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buffer);

      disk._interface = mapBusType(desc->BusType);
      if (desc->ProductIdOffset) {
        disk._model = buffer + desc->ProductIdOffset;
      }
      if (desc->VendorIdOffset) {
        disk._vendor = buffer + desc->VendorIdOffset;
      } else {
        static const std::vector<std::string> known_vendors = {"KIOXIA",  "SAMSUNG", "WESTERN DIGITAL", "WD",
                                                               "SEAGATE", "INTEL",   "CRUCIAL",         "KINGSTON"};
        std::string upper_model(disk._model);
        std::transform(disk._model.begin(), disk._model.end(), upper_model.begin(),
                       [](const char c) { return static_cast<char>(std::toupper(static_cast<int>(c))); });
        for (const auto& vendor : known_vendors) {
          if (upper_model.find(vendor) != std::string::npos) {
            disk._vendor = vendor;
            break;
          }
        }
      }
      if (desc->SerialNumberOffset) {
        disk._serial_number = buffer + desc->SerialNumberOffset;
      }
    }

    disk._media_type = getDiskMediaType(hDevice, disk._interface);

    DISK_GEOMETRY_EX geometry;
    if (DeviceIoControl(hDevice, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, nullptr, 0, &geometry, sizeof(geometry),
                        &bytesReturned, nullptr)) {
      disk._size_bytes = geometry.DiskSize.QuadPart;
    }

    if (diskToVolumes.count(i)) {
      disk._mount_points = diskToVolumes[i];
    }

    CloseHandle(hDevice);
    disks.push_back(std::move(disk));
  }
  return disks;
}

}  // namespace hwinfo

#endif  // HWINFO_WINDOWS

#ifdef HWINFO_WINDOWS

#ifndef __MINGW32__
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "setupapi.lib")
#endif

namespace hwinfo {

namespace {

using PciDeviceId = std::pair<UINT, UINT>;

inline std::optional<PciDeviceId> parsePciDeviceId(const std::wstring& hardware_id) {
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

inline std::map<PciDeviceId, std::size_t> getPhysicalGpuCounts() {
  std::map<PciDeviceId, std::size_t> result;
  const HDEVINFO device_info = SetupDiGetClassDevsW(&GUID_DEVCLASS_DISPLAY, nullptr, nullptr, DIGCF_PRESENT);
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

inline std::vector<GPU> getAllGPUs() {
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

#ifdef HWINFO_WINDOWS

namespace hwinfo {

inline MainBoard::MainBoard() {
  utils::WMI::_WMI wmi;
  const std::wstring query_string(L"SELECT Manufacturer, Product, Version, SerialNumber FROM Win32_BaseBoard");
  bool success = wmi.execute_query(query_string);
  if (!success) {
    return;
  }
  ULONG u_return = 0;
  IWbemClassObject* obj = nullptr;
  wmi.enumerator->Next((long)WBEM_INFINITE, 1, &obj, &u_return);
  if (!u_return) {
    return;
  }
  VARIANT vt_prop;
  HRESULT hr;
  hr = obj->Get(L"Manufacturer", 0, &vt_prop, nullptr, nullptr);
  if (SUCCEEDED(hr) && (V_VT(&vt_prop) == VT_BSTR)) {
    _vendor = utils::wstring_to_std_string(vt_prop.bstrVal);
  }
  hr = obj->Get(L"Product", 0, &vt_prop, nullptr, nullptr);
  if (SUCCEEDED(hr) && (V_VT(&vt_prop) == VT_BSTR)) {
    _name = utils::wstring_to_std_string(vt_prop.bstrVal);
  }
  hr = obj->Get(L"Version", 0, &vt_prop, nullptr, nullptr);
  if (SUCCEEDED(hr) && (V_VT(&vt_prop) == VT_BSTR)) {
    _version = utils::wstring_to_std_string(vt_prop.bstrVal);
  }
  hr = obj->Get(L"SerialNumber", 0, &vt_prop, nullptr, nullptr);
  if (SUCCEEDED(hr) && (V_VT(&vt_prop) == VT_BSTR)) {
    _serial_number = utils::wstring_to_std_string(vt_prop.bstrVal);
  }
  VariantClear(&vt_prop);
  obj->Release();
}

}  // namespace hwinfo

#endif  // HWINFO_WINDOWS
#ifdef HWINFO_WINDOWS

namespace hwinfo {

inline std::vector<Network> getAllNetworks() {
  utils::WMI::_WMI wmi;
  const std::wstring query_string(
      L"SELECT InterfaceIndex, IPAddress, Description, MACAddress "
      L"FROM Win32_NetworkAdapterConfiguration");
  bool success = wmi.execute_query(query_string);
  if (!success) {
    return {};
  }
  std::vector<Network> networks;

  ULONG u_return = 0;
  IWbemClassObject* obj = nullptr;
  while (wmi.enumerator) {
    wmi.enumerator->Next((long)WBEM_INFINITE, 1, &obj, &u_return);
    if (!u_return) {
      break;
    }
    Network network;
    VARIANT vt_prop;
    HRESULT hr;
    hr = obj->Get(L"InterfaceIndex", 0, &vt_prop, nullptr, nullptr);
    if (SUCCEEDED(hr)) {
      network._index = std::to_string(vt_prop.uintVal);
    }
    hr = obj->Get(L"IPAddress", 0, &vt_prop, nullptr, nullptr);
    if (SUCCEEDED(hr)) {
      if (vt_prop.vt == (VT_ARRAY | VT_BSTR)) {
        LONG lbound, ubound;
        SafeArrayGetLBound(vt_prop.parray, 1, &lbound);
        SafeArrayGetUBound(vt_prop.parray, 1, &ubound);
        std::string ipv4, ipv6;
        for (LONG i = lbound; i <= ubound; ++i) {
          BSTR bstr;
          SafeArrayGetElement(vt_prop.parray, &i, &bstr);
          std::wstring ws(bstr, SysStringLen(bstr));
          std::string ip = utils::wstring_to_std_string(ws);
          if (ip.find(':') != std::string::npos) {
            if (ip.find("fe80::") == 0) {
              ipv6 = ip;
            } else {
              ipv6 = "";
            }
          } else {
            ipv4 = ip;
          }
          SysFreeString(bstr);
        }
        network._ip4 = ipv4;
        network._ip6 = ipv6;
      }
    }
    hr = obj->Get(L"Description", 0, &vt_prop, nullptr, nullptr);
    if (SUCCEEDED(hr)) {
      if (vt_prop.vt == VT_BSTR) {
        network._description = utils::wstring_to_std_string(vt_prop.bstrVal);
      }
    }
    hr = obj->Get(L"MACAddress", 0, &vt_prop, nullptr, nullptr);
    if (SUCCEEDED(hr)) {
      if (vt_prop.vt == VT_BSTR) {
        network._mac = utils::wstring_to_std_string(vt_prop.bstrVal);
      }
    }
    VariantClear(&vt_prop);
    obj->Release();
    networks.push_back(std::move(network));
  }
  return networks;
}

}  // namespace hwinfo

#endif  // HWINFO_WINDOWS

#ifdef HWINFO_WINDOWS

#define STATUS_SUCCESS 0x00000000

namespace hwinfo {

inline OS::OS() {
  {
    // Get endian. This is platform independent...
    char16_t dummy = 0x0102;
    _bigEndian = ((char*)&dummy)[0] == 0x01;
    _littleEndian = ((char*)&dummy)[0] == 0x02;
  }
  utils::WMI::_WMI wmi;
  const std::wstring query_string(L"SELECT Caption, OSArchitecture, BuildNumber, Version FROM Win32_OperatingSystem");
  bool success = wmi.execute_query(query_string);
  if (!success) {
    return;
  }
  ULONG u_return = 0;
  IWbemClassObject* obj = nullptr;
  wmi.enumerator->Next((long)WBEM_INFINITE, 1, &obj, &u_return);
  if (!u_return) {
    return;
  }
  VARIANT vt_prop;
  HRESULT hr;
  hr = obj->Get(L"Caption", 0, &vt_prop, nullptr, nullptr);
  if (SUCCEEDED(hr) && (V_VT(&vt_prop) == VT_BSTR)) {
    _name = utils::wstring_to_std_string(vt_prop.bstrVal);
  }
  hr = obj->Get(L"OSArchitecture", 0, &vt_prop, nullptr, nullptr);
  if (SUCCEEDED(hr) && (V_VT(&vt_prop) == VT_BSTR)) {
    _64bit = utils::wstring_to_std_string(vt_prop.bstrVal).find("64") != std::string::npos;
    _32bit = !_64bit;
  }
  hr = obj->Get(L"BuildNumber", 0, &vt_prop, nullptr, nullptr);
  if (SUCCEEDED(hr) && (V_VT(&vt_prop) == VT_BSTR)) {
    _version = utils::wstring_to_std_string(vt_prop.bstrVal);
  }
  hr = obj->Get(L"Version", 0, &vt_prop, nullptr, nullptr);
  if (SUCCEEDED(hr) && (V_VT(&vt_prop) == VT_BSTR)) {
    _kernel = utils::wstring_to_std_string(vt_prop.bstrVal);
  }
  VariantClear(&vt_prop);
  obj->Release();
}

}  // namespace hwinfo

#endif  // HWINFO_WINDOWS

#ifdef HWINFO_WINDOWS

namespace hwinfo {

inline Memory::Memory() {
  utils::WMI::_WMI wmi;
  const std::wstring query_string(
      L"SELECT Capacity, ConfiguredClockSpeed, Manufacturer, SerialNumber, PartNumber FROM Win32_PhysicalMemory");
  bool success = wmi.execute_query(query_string);
  if (!success) {
    return;
  }
  ULONG u_return = 0;
  IWbemClassObject* obj = nullptr;
  std::vector<Memory> rams;
  int id = 0;
  while (wmi.enumerator) {
    wmi.enumerator->Next((long)WBEM_INFINITE, 1, &obj, &u_return);
    if (!u_return) {
      break;
    }
    VARIANT vt_prop;
    HRESULT hr;
    Memory::Module module;
    module.id = id++;
    hr = obj->Get(L"Manufacturer", 0, &vt_prop, nullptr, nullptr);
    if (SUCCEEDED(hr) && (V_VT(&vt_prop) == VT_BSTR)) {
      module.vendor = utils::wstring_to_std_string(vt_prop.bstrVal);
    }
    hr = obj->Get(L"partNumber", 0, &vt_prop, nullptr, nullptr);
    if (SUCCEEDED(hr) && (V_VT(&vt_prop) == VT_BSTR)) {
      module.model = utils::wstring_to_std_string(vt_prop.bstrVal);
      // TODO: One expects an actual name of the RAM but wmi does not provide such a property...
      //       The "Name"-property of WMI returns "PhysicalMemory".
      module.name = std::string(module.vendor + " " + module.model);
    }
    hr = obj->Get(L"Capacity", 0, &vt_prop, nullptr, nullptr);
    if (SUCCEEDED(hr) && (V_VT(&vt_prop) == VT_BSTR)) {
      module._size_bytes = std::stoll(utils::wstring_to_std_string(vt_prop.bstrVal));
    }
    hr = obj->Get(L"ConfiguredClockSpeed", 0, &vt_prop, nullptr, nullptr);
    if (SUCCEEDED(hr) && (V_VT(&vt_prop) == VT_I4)) {
      module.frequency_hz = static_cast<int64_t>(vt_prop.intVal) * 1000 * 1000;
    }
    hr = obj->Get(L"SerialNumber", 0, &vt_prop, nullptr, nullptr);
    if (SUCCEEDED(hr) && (V_VT(&vt_prop) == VT_BSTR)) {
      module.serial_number = utils::wstring_to_std_string(vt_prop.bstrVal);
    }
    VariantClear(&vt_prop);
    obj->Release();
    _modules.push_back(std::move(module));
  }
}

inline uint64_t Memory::size() const {
  uint64_t sum = 0;
  for (const auto& module : _modules) {
    sum += module._size_bytes;
  }
  return sum;
}

inline uint64_t Memory::free() const {
  auto res = utils::WMI::query<std::string>(L"Win32_OperatingSystem", L"FreePhysicalMemory");
  if (res.empty()) {
    return 0;
  }
  return std::stoll(res.front()) * 1024;
}

inline uint64_t Memory::available() const {
  // TODO: Get actual available memory size...
  return free();
}

}  // namespace hwinfo

#endif  // HWINFO_WINDOWS
#elif defined(HWINFO_APPLE)

#ifdef HWINFO_APPLE

namespace hwinfo {

// Helper functions to reduce code duplication
namespace {

// Check if the system is running on Apple Silicon
inline bool isAppleSilicon() {
  auto machine = utils::getSysctlString("hw.machine");
  return machine.find("arm64") != std::string::npos;
}

inline bool isPowerPC() {
  int cputype = utils::getSysctlValue<int>("hw.cputype", -1);
  return cputype == 18;  // 18 is CPU_TYPE_POWERPC
}

}  // anonymous namespace

inline std::string getVendor() {
  auto vendor = utils::getSysctlString("machdep.cpu.vendor", "<unknown>");

  if (vendor == "<unknown>" && isAppleSilicon()) {
    return "Apple";
  }

  if (vendor == "<unknown>" && isPowerPC()) {
    return "IBM";
  }
  return vendor;
}

inline std::string getModelName() {
  std::string model = utils::getSysctlString("machdep.cpu.brand_string", "<unknown>");
  if (model == "<unknown>" && isAppleSilicon()) {
    model = "Apple Silicon";
  }
  if (model == "<unknown>" && isPowerPC()) {
    int cpusubtype = utils::getSysctlValue<int>("hw.cpusubtype", -1);
    switch (cpusubtype) {
      case 100:
        model = "PowerPC G5 (970)";
        break;
      case 11:
        model = "PowerPC G4 (7450)";
        break;
      case 10:
        model = "PowerPC G4 (7400)";
        break;
      case 9:
        model = "PowerPC G3 (750)";
        break;
      case 1:
        model = "PowerPC 601";
        break;
      default:
        model = "PowerPC (unknown subtype: " + std::to_string(cpusubtype) + ")";
    }
  }
  return model;
}

inline int getNumPhysicalCores() { return utils::getSysctlValue<int>("hw.physicalcpu", 0); }

inline int getNumLogicalCores() { return utils::getSysctlValue<int>("hw.logicalcpu", 0); }

[[maybe_unused]] inline int64_t getL1CacheSize_Bytes() { return utils::getSysctlValue<int64_t>("hw.l1dcachesize", -1); }

[[maybe_unused]] inline int64_t getL2CacheSize_Bytes() { return utils::getSysctlValue<int64_t>("hw.l2cachesize", -1); }

[[maybe_unused]] inline int64_t getL3CacheSize_Bytes() { return utils::getSysctlValue<int64_t>("hw.l3cachesize", -1); }

inline std::vector<CPU> getAllCPUs() {
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

#ifdef HWINFO_APPLE

namespace hwinfo {

/**
  Converts a CFStringRef to a std::string
 */
inline std::string cf_to_std(CFStringRef cfString) {
  if (cfString == nullptr) {
    return "<unknown>";
  }

  CFIndex length = CFStringGetLength(cfString);
  CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;

  // Initialize std::string with maxSize and fill with null characters
  auto out = std::string(maxSize, '\0');

  // Fill std::string with the actual string
  auto success = CFStringGetCString(cfString, const_cast<char*>(out.data()), maxSize, kCFStringEncodingUTF8);

  if (!success) {
    return "<unknown>";
  }

  // Resize the string to the actual length
  out.resize(strlen(out.c_str()));
  return out;
}

/**
  Converts a CFNumberRef to a number of type ReturnType
 */
template <typename NumberType>
inline NumberType cf_to_std(CFNumberRef raw, CFNumberType cfNumberEnum) {
  if (raw == nullptr) {
    return NumberType();
  }

  NumberType out;
  CFNumberGetValue(raw, cfNumberEnum, &out);

  return out;
}

inline int64_t cf_to_std(CFNumberRef raw) { return cf_to_std<int64_t>(raw, kCFNumberSInt64Type); }

template <typename ReturnType, typename CFType>
inline ReturnType getIORegistryProperty(io_object_t service, CFStringRef key) {
  // Get the property from I/O Registry
  auto raw = static_cast<CFType>(IORegistryEntryCreateCFProperty(service, key, kCFAllocatorDefault, 0));

  // Convert the property to a output type
  ReturnType out = cf_to_std(raw);

  // Release the property
  if (raw) {
    CFRelease(raw);
  };

  return out;
}

inline std::optional<bool> parseSolidStateProperty(CFTypeRef property) {
  if (property == nullptr) {
    return std::nullopt;
  }
  if (CFGetTypeID(property) == CFBooleanGetTypeID()) {
    return CFBooleanGetValue(static_cast<CFBooleanRef>(property));
  }
  if (CFGetTypeID(property) == CFStringGetTypeID()) {
    std::string medium = cf_to_std(static_cast<CFStringRef>(property));
    std::transform(medium.begin(), medium.end(), medium.begin(),
                   [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (medium.find("solid state") != std::string::npos || medium == "ssd") {
      return true;
    }
    if (medium.find("rotational") != std::string::npos || medium.find("hard disk") != std::string::npos) {
      return false;
    }
  }
  return std::nullopt;
}

inline hwinfo::Disk::MediaType getDiskMediaType(io_object_t service) {
  CFTypeRef property =
      IORegistryEntrySearchCFProperty(service, kIOServicePlane, CFSTR("Solid State"), kCFAllocatorDefault,
                                      kIORegistryIterateRecursively | kIORegistryIterateParents);
  if (property != nullptr) {
    const auto result = parseSolidStateProperty(property);
    CFRelease(property);
    if (result.has_value()) {
      return *result ? hwinfo::Disk::MediaType::SOLID_STATE : hwinfo::Disk::MediaType::ROTATIONAL;
    }
  }

  property =
      IORegistryEntrySearchCFProperty(service, kIOServicePlane, CFSTR("Device Characteristics"), kCFAllocatorDefault,
                                      kIORegistryIterateRecursively | kIORegistryIterateParents);
  if (property == nullptr) {
    return hwinfo::Disk::MediaType::UNKNOWN;
  }

  std::optional<bool> result;
  if (CFGetTypeID(property) == CFDictionaryGetTypeID()) {
    const auto dictionary = static_cast<CFDictionaryRef>(property);
    result = parseSolidStateProperty(CFDictionaryGetValue(dictionary, CFSTR("Medium Type")));
  }
  CFRelease(property);
  if (!result.has_value()) {
    return hwinfo::Disk::MediaType::UNKNOWN;
  }
  return *result ? hwinfo::Disk::MediaType::SOLID_STATE : hwinfo::Disk::MediaType::ROTATIONAL;
}

/**
 * Extracts the base disk name (e.g. "disk3") from a
 * BSD device name like "disk3s1s1".
 *
 * @param bsdName A string such as "disk3s1s1"
 * @return "disk3" if bsdName starts with "disk", otherwise returns bsdName
 */
inline std::string parseBaseDiskName(const std::string& bsdName) {
  // Check if it starts with "disk"
  if (bsdName.rfind("disk", 0) == 0) {
    // skip the first 4 characters ("disk")
    size_t pos = 4;
    // consume all digits (e.g. "3") until we hit a non-digit
    while (pos < bsdName.size() && std::isdigit(static_cast<unsigned char>(bsdName[pos]))) {
      pos++;
    }
    // return the substring that includes "disk" and any trailing digits
    return bsdName.substr(0, pos);
  }
  // fallback if it doesn't start with "disk"
  return bsdName;
}

/**
 * Builds a mapping from BSD device names to mount points using getfsstat().
 *
 * For each mounted device (e.g. "/dev/disk3s1s1" -> "/"), we:
 *   1. Strip "/dev/" -> "disk3s1s1".
 *   2. Extract the base disk name (e.g. "disk3").
 *   3. Store "disk3s1s1" -> "/" in mountMap.
 *   4. Also link "disk3" to the same mount point if not already linked.
 *
 * This lets us find a container disk's mount (e.g. "disk3" -> "/").
 */
inline std::unordered_map<std::string, std::string> getBSDToMountPointMapping() {
  std::unordered_map<std::string, std::string> mountMap;             // partition -> mount point
  std::unordered_map<std::string, std::string> baseDiskToPartition;  // diskX -> first found "diskXsY"

  int mountCount = getfsstat(nullptr, 0, MNT_NOWAIT);
  if (mountCount <= 0) {
    return mountMap;
  }

  std::vector<struct statfs> mountInfo(mountCount);
  if (getfsstat(mountInfo.data(), mountCount * sizeof(struct statfs), MNT_NOWAIT) == -1) {
    return mountMap;
  }

  for (const auto& entry : mountInfo) {
    std::string fullBSDName = entry.f_mntfromname;  // e.g. "/dev/disk3s1s1"
    std::string mountPath = entry.f_mntonname;      // e.g. "/"

    // Remove the "/dev/" prefix if present
    if (fullBSDName.rfind("/dev/", 0) == 0) {
      fullBSDName.erase(0, 5);
    }

    // Extract the base disk (e.g. "disk3") from e.g. "disk3s1s1"
    std::string baseDisk = parseBaseDiskName(fullBSDName);

    // Store partition -> mount
    mountMap[fullBSDName] = mountPath;

    // Link "disk3" to the first partition we see, if not already linked
    if (baseDiskToPartition.find(baseDisk) == baseDiskToPartition.end()) {
      baseDiskToPartition[baseDisk] = fullBSDName;
    }
  }

  // Link each base disk "diskX" to the same mount as its first partition "diskXsY"
  for (const auto& [disk, partition] : baseDiskToPartition) {
    auto it = mountMap.find(partition);
    if (it != mountMap.end()) {
      mountMap[disk] = it->second;
    }
  }

  return mountMap;
}

/**
 * Retrieves the free disk space (in bytes) for a given mount point
 * by calling statfs().
 *
 * @param mountPoint The path at which the filesystem is mounted (e.g. "/")
 * @return The free space in bytes, or (uint64_t)-1 on error
 */
inline uint64_t getFreeDiskSpace(const std::string& mountPoint) {
  struct statfs fsStats;

  if (statfs(mountPoint.c_str(), &fsStats) == 0) {
    uint64_t freeSpace = static_cast<uint64_t>(fsStats.f_bavail) * fsStats.f_bsize;
    return freeSpace;
  } else {
    return static_cast<uint64_t>(-1);
  }
}

// Retrieves disk information using I/O Kit
inline std::vector<Disk> getAllDisks() {
  std::vector<Disk> disks;

  // Build a map from BSD devices (diskXsY) and base disks (diskX) to mount points
  auto mountMap = getBSDToMountPointMapping();

  CFMutableDictionaryRef matchingDict = IOServiceMatching(kIOMediaClass);
  CFDictionaryAddValue(matchingDict, CFSTR(kIOMediaWholeKey), kCFBooleanTrue);

  io_iterator_t iter;
  if (IOServiceGetMatchingServices(0, matchingDict, &iter) == KERN_SUCCESS) {
    int i_disk = 0;
    while (true) {
      auto service = IOIteratorNext(iter);
      if (service == 0) {
        break;
      }

      Disk disk;
      disk._id = i_disk;

      // Retrieve the BSD name (e.g. "disk3")
      std::string bsdName = getIORegistryProperty<std::string, CFStringRef>(service, CFSTR(kIOBSDNameKey));

      // Get disk name
      char model[128];
      if (IORegistryEntryGetName(service, model) != KERN_SUCCESS) {
        disk._model = "<unknown>";
      } else {
        disk._model = model;
      }

      // Guess vendor based on model
      if (disk._model.find("APPLE") != std::string::npos || disk._model.find("Apple") != std::string::npos) {
        disk._vendor = "Apple";
      } else {
        disk._vendor = "<unknown>";
      }

      disk._serial_number = getIORegistryProperty<std::string, CFStringRef>(service, CFSTR(kIOMediaUUIDKey));

      disk._size_bytes = getIORegistryProperty<int64_t, CFNumberRef>(service, CFSTR(kIOMediaSizeKey));

      disk._media_type = getDiskMediaType(service);

      // If there's no BSD name, we can't look it up in the mount map
      if (!bsdName.empty()) {
        // Look up this BSD device in the mountMap
        if (auto it = mountMap.find(bsdName); it != mountMap.end()) {
          // Get free space for the found mount point
          const std::string& mountPoint = it->second;
          disk._size_bytes = getFreeDiskSpace(mountPoint);

          disk._mount_points.push_back(mountPoint);
        }
      }

      disks.push_back(std::move(disk));

      IOObjectRelease(service);

      i_disk++;
    }
    IOObjectRelease(iter);
  }
  return disks;
}

}  // namespace hwinfo

#endif  // HWINFO_APPLE

#ifdef HWINFO_APPLE

namespace hwinfo {

inline std::vector<GPU> getAllGPUs() {
  std::vector<GPU> gpus{};
  // TODO: implement
  return gpus;
}

}  // namespace hwinfo

#endif  // HWINFO_APPLE

#ifdef HWINFO_APPLE

#if defined(__MAC_12_0) && __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_12_0
#define SAFE_IO_MAIN_PORT kIOMainPortDefault
#else
#define SAFE_IO_MAIN_PORT kIOMasterPortDefault
#endif

namespace hwinfo {

inline std::string get_mainboard_property(CFStringRef property_name) {
  auto platformExpert = IOServiceGetMatchingService(SAFE_IO_MAIN_PORT, IOServiceMatching("IOPlatformExpertDevice"));
  if (!platformExpert) {
    return "<unknown>";
  }

  auto propertyRef = IORegistryEntryCreateCFProperty(platformExpert, property_name, kCFAllocatorDefault, 0);

  IOObjectRelease(platformExpert);

  if (!propertyRef) {
    return "<unknown>";
  }

  std::string result;

  if (CFTypeID typeID = CFGetTypeID(propertyRef); typeID == CFStringGetTypeID()) {
    auto stringRef = static_cast<CFStringRef>(propertyRef);
    auto length = CFStringGetLength(stringRef);
    auto maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;

    auto buffer = std::string(maxSize, '\0');
    if (CFStringGetCString(stringRef, buffer.data(), maxSize, kCFStringEncodingUTF8)) {
      result = buffer.c_str();  // Remove trailing nulls
    }
  } else if (typeID == CFDataGetTypeID()) {
    const auto dataRef = static_cast<CFDataRef>(propertyRef);
    const auto length = CFDataGetLength(dataRef);
    const auto bytes = CFDataGetBytePtr(dataRef);
    result = std::string(reinterpret_cast<const char*>(bytes), length);

    if (!result.empty() && result.back() == '\0') {
      result.pop_back();
    }
  }

  CFRelease(propertyRef);
  return result.empty() ? "<unknown>" : result;
}

inline MainBoard::MainBoard() {
  _vendor = get_mainboard_property(CFSTR("manufacturer"));
  _name = "<unknown>";
  _version = "<unknown>";
  _serial_number = get_mainboard_property(CFSTR(kIOPlatformSerialNumberKey));
}

}  // namespace hwinfo

#endif  // HWINFO_APPLE
#ifdef HWINFO_APPLE
namespace hwinfo {
inline std::vector<Network> getAllNetworks() {
  std::vector<Network> networks;
  return networks;
}
}  // namespace hwinfo

#endif  // HWINFO_APPLE

#ifdef HWINFO_APPLE

inline std::string getOSVersionFromPlist() {
  std::ifstream file("/System/Library/CoreServices/SystemVersion.plist");
  if (!file.is_open()) return "";

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();

  std::smatch match;
  std::regex version_regex("<key>ProductVersion</key>\\s*<string>([^<]+)</string>");
  if (std::regex_search(content, match, version_regex)) {
    return match[1];
  }
  return "";
}

namespace hwinfo {

inline std::string getMarketingName(const std::string& version) {
  size_t dotPos = version.find('.');
  if (dotPos == std::string::npos) {
    return "";
  }

  int majorVersion = 0;
  try {
    majorVersion = std::stoi(version.substr(0, dotPos));
  } catch (...) {
    return "";
  }

  // map major version to marketing name
  switch (majorVersion) {
    case 26:
      return "Tahoe";
    case 15:
      return "Sequoia";
    case 14:
      return "Sonoma";
    case 13:
      return "Ventura";
    case 12:
      return "Monterey";
    case 11:
      return "Big Sur";
    case 10: {
      // handle 10.x versions - need to check minor version
      size_t secondDot = version.find('.', dotPos + 1);
      std::string minorStr = (secondDot != std::string::npos) ? version.substr(dotPos + 1, secondDot - dotPos - 1)
                                                              : version.substr(dotPos + 1);
      try {
        switch (std::stoi(minorStr)) {
          case 15:
            return "Catalina";
          case 14:
            return "Mojave";
          case 13:
            return "High Sierra";
          case 12:
            return "Sierra";
          case 11:
            return "El Capitan";
          case 10:
            return "Yosemite";
          case 9:
            return "Mavericks";
          case 8:
            return "Mountain Lion";
          case 7:
            return "Lion";
          case 6:
            return "Snow Leopard";
          case 5:
            return "Leopard";
          case 4:
            return "Tiger";
          case 3:
            return "Panther";
          case 2:
            return "Jaguar";
          case 1:
            return "Puma";
          case 0:
            return "Cheetah";
          default:
            return "";
        }
      } catch (...) {
        return "";
      }
    }
    default:
      return "";
  }
}

inline OS::OS() {
  _name = "macOS";

  // Get kernel name and version
  _kernel = utils::getSysctlString("kern.ostype", "<unknown name>");
  _kernel += " " + utils::getSysctlString("kern.osrelease", "<unknown version>");

  // get OS name and build version
  _version = getOSVersionFromPlist();
  if (_version.empty()) {
    _version = utils::getSysctlString("kern.osproductversion", "<unknown>");
  }
  std::string build = utils::getSysctlString("kern.osversion", "<unknown build>");
  _version = _version + " (" + build + ")";
  // add marketing name if we can determine it
  if (std::string marketingName = getMarketingName(_version); !marketingName.empty()) {
    _name = _name + " " + marketingName;
  }
  // determine endianess
  const int byteorder = utils::getSysctlValue("hw.byteorder", 0);
  _bigEndian = (byteorder == 4321);
  _littleEndian = (byteorder == 1234);

#if defined(__x86_64__) || defined(__aarch64__) || defined(__ppc64__)
  _64bit = true;
#else
  _64bit = false;
#endif
  _32bit = !_64bit;
}

}  // namespace hwinfo

#endif  // HWINFO_APPLE

#ifdef HWINFO_APPLE

namespace hwinfo {

inline uint64_t getMemSize() {
  uint64_t memSize;
  size_t size = sizeof(memSize);

  if (sysctlbyname("hw.memsize", &memSize, &size, nullptr, 0) == 0) {
    return memSize;
  }

  return 0;
}

inline Memory::Memory() {
  // TODO: get information for actual memory modules (DIMM)
}

inline uint64_t Memory::size() const { return getMemSize(); }

inline uint64_t Memory::free() const {
  vm_statistics64_data_t vmStats;
  mach_msg_type_number_t infoCount = HOST_VM_INFO64_COUNT;
  kern_return_t kernReturn =
      host_statistics64(mach_host_self(), HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vmStats), &infoCount);

  if (kernReturn == KERN_SUCCESS) {
    vm_size_t pageSize;
    host_page_size(mach_host_self(), &pageSize);

    int64_t totalMemory = getMemSize();
    if (totalMemory == -1) return -1;

    const int64_t appMemory =
        static_cast<int64_t>(vmStats.internal_page_count - vmStats.purgeable_count) * static_cast<int64_t>(pageSize);
    const int64_t wiredMemory = static_cast<int64_t>(vmStats.wire_count) * static_cast<int64_t>(pageSize);
    const int64_t compressedMemory =
        static_cast<int64_t>(vmStats.compressor_page_count) * static_cast<int64_t>(pageSize);

    const int64_t usedMemory = appMemory + wiredMemory + compressedMemory;
    return totalMemory - usedMemory;
  }

  return 0;
}

inline uint64_t Memory::available() const {
  int64_t usableMemSize;
  size_t size = sizeof(usableMemSize);

  if (sysctlbyname("hw.memsize_usable", &usableMemSize, &size, nullptr, 0) == 0) {
    return usableMemSize;
  }

  return 0;
}

}  // namespace hwinfo

#endif  // HWINFO_APPLE
#elif defined(HWINFO_UNIX)

#ifdef HWINFO_UNIX

namespace hwinfo {

inline uint64_t readSysfsUint(const std::string& path) {
  std::ifstream f(path);
  uint64_t val = 0;
  if (f >> val) {
    return val;
  }
  return 0;
}

inline std::uint64_t read_cache_size(const std::string& path) {
  std::ifstream f(path);
  if (!f.is_open()) {
    return 0;
  }
  std::uint64_t value;
  std::string unit;
  if (f >> value >> unit) {
    if (unit == "K") {
      return value * static_cast<std::uint64_t>(unit::IECPrefix::KIBI);
    } else if (unit == "M") {
      return value * static_cast<std::uint64_t>(unit::IECPrefix::MEBI);
    } else if (unit == "G") {
      return value * static_cast<std::uint64_t>(unit::IECPrefix::GIBI);
    }
    return value;
  }
  return 0;
}

inline bool has_smt(const std::string& core_path) {
  std::string path = core_path + "/topology/thread_siblings_list";
  std::ifstream tsf(path);
  std::string siblings;
  return (tsf >> siblings && siblings.find_first_of('-') != std::string::npos);
}

namespace unix_os::cpu {

inline std::map<std::uint32_t, std::map<std::uint32_t, std::map<std::string, std::string>>> parse_cpuinfo_file(
    const std::string& data) {
  std::vector<std::map<std::string, std::string>> blocks;
  {
    std::string fallback_vendor;
    std::string fallback_model;
    std::vector<std::string> sections = utils::split(data, "\n\n");
    for (const auto& section : sections) {
      std::map<std::string, std::string> current_block;
      for (const auto& line : utils::split(section, "\n")) {
        std::vector<std::string> pair = utils::split(line, ":");
        if (pair.size() != 2) {
          continue;
        }
        std::string key = std::move(pair[0]);
        std::string value = std::move(pair[1]);
        utils::strip(key);
        utils::strip(value);
        current_block[key] = value;
      }
      {
        // On some ARM systems, Processor and Hardware are defined globally for all listed cores
        if (current_block.count("Processor")) {
          fallback_model = current_block["Processor"];
        } else if (current_block.count("Hardware")) {
          fallback_vendor = current_block["Hardware"];
        } else if (current_block.count("Model")) {
          fallback_model = current_block["Model"];
        }
      }
      blocks.emplace_back(std::move(current_block));
    }

    std::map<std::uint32_t, std::map<std::uint32_t, std::map<std::string, std::string>>> cpus;
    for (auto& block : blocks) {
      if (block.count("processor") == 0) {
        // skip as this block is not an entry for a core
        continue;
      }
      std::uint32_t processor_id = std::stoi(block["processor"]);
      std::uint32_t physical_id = 0;
      if (block.count("physical id")) {
        physical_id = std::stoi(block["physical id"]);
      }
      block["fallback_model"] = fallback_model;
      block["fallback_vendor"] = fallback_vendor;
      auto& socket = cpus[physical_id];
      socket[processor_id].insert(std::move_iterator(block.begin()), std::move_iterator(block.end()));
    }
    return cpus;
  }
}

inline std::map<std::uint32_t, unix_os::cpu::CPU> parse_cpus(
    const std::map<std::uint32_t, std::map<std::uint32_t, std::map<std::string, std::string>>>& sockets) {
  if (sockets.empty()) {
    return {};
  }
  std::map<std::uint32_t, unix_os::cpu::CPU> cpus;
  for (const auto& [socket_id, processors] : sockets) {
    unix_os::cpu::CPU cpu;
    cpu.socket_id = socket_id;
    for (const auto& [processor_id, processor] : processors) {
      std::uint32_t core_id;
      if (processor.count("core id")) {
        core_id = std::stoi(processor.at("core id"));
      } else {
        core_id = processor_id;
      }
      if (cpu.cores.count(core_id)) {
        continue;
      }
      {  // CPU information
        if (processor.count("vendor_id")) {
          cpu.vendor = processor.at("vendor_id");
        } else {
          if (cpu.vendor.empty()) {
            cpu.vendor = processor.at("fallback_vendor");
          }
        }
        if (processor.count("model name")) {
          cpu.model = processor.at("model name");
        } else {
          if (cpu.model.empty()) {
            cpu.model = processor.at("fallback_model");
          }
        }
      }
      {  // core information
        unix_os::cpu::CPU::Core& core = cpu.cores[core_id];
        const std::string core_path = "/sys/devices/system/cpu/cpu" + std::to_string(processor_id);
        core.id = processor_id;
        core.smt = has_smt(core_path);
        core.cache_bytes[0] = read_cache_size(core_path + "/cache/index0/size");
        core.cache_bytes[1] = read_cache_size(core_path + "/cache/index1/size");
        core.cache_bytes[2] = read_cache_size(core_path + "/cache/index2/size");
        core.cache_bytes[3] = read_cache_size(core_path + "/cache/index3/size");
        if (processor.count("flags")) {
          core.flags = utils::split(processor.at("flags"), " ");
        } else if (processor.count("Features")) {
          core.flags = utils::split(processor.at("Features"), " ");
        } else if (processor.count("isa")) {
          core.flags = utils::split(processor.at("isa"), " ");
        }
        core.regular_frequency_hz = readSysfsUint(core_path + "/cpufreq/base_frequency");
        if (core.regular_frequency_hz == 0 && processor.count("cpu MHz")) {
          core.regular_frequency_hz = std::stod(processor.at("cpu MHz")) * unit::SiPrefix::MEGA;
        }
        core.max_frequency_hz = readSysfsUint(core_path + "/cpufreq/cpuinfo_max_freq");
      }
    }
    cpus[socket_id] = std::move(cpu);
  }
  return cpus;
}

}  // namespace unix_os::cpu

inline std::vector<CPU> getAllCPUs() {
  std::ifstream file("/proc/cpuinfo");
  if (!file.is_open()) return {};

  std::string data(std::istreambuf_iterator<char>{file}, {});

  auto blocks = unix_os::cpu::parse_cpuinfo_file(data);
  auto parsed_cpus = unix_os::cpu::parse_cpus(blocks);

  std::vector<CPU> result;
  result.reserve(parsed_cpus.size());
  for (const auto& [socket_id, parsed_cpu] : parsed_cpus) {
    CPU cpu;
    cpu._vendor = parsed_cpu.vendor;
    cpu._modelName = parsed_cpu.model;
    cpu._id = socket_id;
    cpu._cores.reserve(parsed_cpus.size());
    for (const auto& [core_id, core] : parsed_cpu.cores) {
      cpu._cores.emplace_back(CPU::Core{
          core.id, CPU::Cache{core.cache_bytes[0], core.cache_bytes[1], core.cache_bytes[2], core.cache_bytes[3]},
          core.regular_frequency_hz, core.max_frequency_hz, core.smt});

      cpu._numLogicalCores += core.smt ? 2 : 1;
      cpu._numPhysicalCores += 1;
    }
    if (!parsed_cpu.cores.empty()) {
      cpu._flags = parsed_cpu.cores.begin()->second.flags;
    }
    result.emplace_back(std::move(cpu));
  }

  return result;
}

}  // namespace hwinfo

#endif  // HWINFO_UNIX

#ifdef HWINFO_UNIX

namespace {

// Linux always considers sectors to be 512 bytes long independently of the devices real block size.
inline constexpr std::size_t block_size = 512;

inline std::string read_file(const std::string& path) {
  std::ifstream file(path);
  if (!file) return {};

  std::string content;
  std::getline(file, content);
  hwinfo::utils::strip(content);
  return content;
}

inline std::string getDiskVendor(const std::filesystem::path& path) {
  std::string result = read_file(path / "device/vendor");
  if (result.empty()) {
    return "<unknown>";
  }
  return result;
}

inline std::string getDiskModel(const std::filesystem::path& path) {
  std::string result = read_file(path / "device/model");
  if (result.empty()) {
    return "<unknown>";
  }
  return result;
}

inline std::string getDiskSerialNumber(const std::filesystem::path& path) {
  std::string result = read_file(path / "device/serial");
  if (result.empty()) {
    return "<unknown>";
  }
  return result;
}

inline uint64_t getDiskSize_Bytes(const std::filesystem::path& path) {
  std::ifstream f(path / "size");
  uint64_t size = 0;
  if (f && f >> size) {
    return size * block_size;
  }
  return 0;
}

inline hwinfo::Disk::MediaType getDiskMediaType(const std::filesystem::path& path,
                                                const hwinfo::Disk::Interface disk_interface) {
  std::ifstream rotationalFile(path / "queue/rotational");
  int rotational = -1;
  if (rotationalFile >> rotational && (rotational == 0 || rotational == 1)) {
    return rotational == 0 ? hwinfo::Disk::MediaType::SOLID_STATE : hwinfo::Disk::MediaType::ROTATIONAL;
  }
  if (disk_interface == hwinfo::Disk::Interface::NVME) {
    return hwinfo::Disk::MediaType::SOLID_STATE;
  }
  return hwinfo::Disk::MediaType::UNKNOWN;
}

inline hwinfo::Disk::Interface getDiskUsbVersion(const std::filesystem::path& disk_sys_path) {
  std::filesystem::path current = std::filesystem::canonical(disk_sys_path);

  while (current != "/" && current.has_parent_path()) {
    if (std::filesystem::exists(current / "speed")) {
      std::string speed_str = read_file(current / "speed");
      if (speed_str.empty()) {
        return hwinfo::Disk::Interface::USB;
      }

      try {
        int speed = std::stoi(speed_str);
        if (speed >= 80000) return hwinfo::Disk::Interface::USB4_80GBit;
        if (speed >= 40000) return hwinfo::Disk::Interface::USB4_40GBit;
        if (speed >= 20000) {
          std::string ver = read_file(current / "version");
          if (ver.find("4.") != std::string::npos) {
            return hwinfo::Disk::Interface::USB4_20GBit;
          }
          return hwinfo::Disk::Interface::USB3_20GBit;
        }
        if (speed >= 10000) return hwinfo::Disk::Interface::USB3_10GBit;
        if (speed >= 5000) return hwinfo::Disk::Interface::USB3_5GBit;
        if (speed == 480) return hwinfo::Disk::Interface::USB2;
        if (speed < 480) return hwinfo::Disk::Interface::USB1;
      } catch (...) {
        return hwinfo::Disk::Interface::USB;
      }
    }
    current = current.parent_path();
  }
  return hwinfo::Disk::Interface::USB;
}

inline hwinfo::Disk::Interface getDiskInterface(const std::filesystem::path& path) {
  std::string subsystem = std::filesystem::canonical(path / "device").string();
  if (subsystem.find("nvme") != std::string::npos) {
    return hwinfo::Disk::Interface::NVME;
  } else if (subsystem.find("usb") != std::string::npos) {
    return getDiskUsbVersion(path);
  } else if (subsystem.find("ata") != std::string::npos) {
    return hwinfo::Disk::Interface::SATA;
  } else if (subsystem.find("scsi") != std::string::npos) {
    return hwinfo::Disk::Interface::SCSI;
  }
  return hwinfo::Disk::Interface::UNKNOWN;
}

}  // anonymous namespace

namespace hwinfo {

inline std::vector<Disk> getAllDisks() {
  std::vector<Disk> disks;

  std::uint32_t id = 0;
  for (const auto& entry : std::filesystem::directory_iterator("/sys/class/block")) {
    std::string name = entry.path().filename().string();
    if (std::filesystem::exists(entry.path() / "partition") || name.find("loop") == 0 || name.find("ram") == 0 ||
        name.find("zram") == 0) {
      // skip partitions, loop devices and virtual devices
      continue;
    }
    try {
      Disk disk;
      disk._id = id++;
      disk._model = getDiskModel(entry.path());
      disk._vendor = getDiskVendor(entry.path());
      disk._serial_number = getDiskSerialNumber(entry.path());
      disk._size_bytes = getDiskSize_Bytes(entry.path());
      disk._interface = getDiskInterface(entry.path());
      disk._media_type = getDiskMediaType(entry.path(), disk._interface);
      for (const auto& sub_entry : std::filesystem::directory_iterator(entry.path())) {
        if (std::filesystem::exists(sub_entry.path() / "partition")) {
          disk._mount_points.emplace_back("/dev/" + sub_entry.path().filename().string());
        }
      }
      disks.emplace_back(std::move(disk));
    } catch (const std::filesystem::filesystem_error& ex) {
      // silently continue for filesystem errors.
    }
  }

  return disks;
}

}  // namespace hwinfo

#endif  // HWINFO_UNIX

#ifdef HWINFO_UNIX

namespace hwinfo {

namespace {

struct PciNames {
  std::string vendor;
  std::string device;
};

inline std::string normalizePciId(std::string id) {
  utils::strip(id);
  if (utils::starts_with(id, "0x")) {
    id.erase(0, 2);
  }
  std::transform(id.begin(), id.end(), id.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return id;
}

inline PciNames lookupPciNames(const std::string& vendor_id, const std::string& device_id) {
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

inline std::string read_drm_by_path(const std::string& path) {
  std::ifstream f_drm(path);
  if (!f_drm) {
    return "";
  }
  std::string ret;
  getline(f_drm, ret);
  return ret;
}

inline std::vector<std::uint64_t> get_frequencies(const std::string drm_path) {
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

inline std::vector<GPU> getAllGPUs() {
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

#ifdef HWINFO_UNIX

namespace hwinfo {

inline std::string get_dmi_by_name(const std::string& name) {
  std::string value;
  std::vector<std::string> candidates = {"/sys/devices/virtual/dmi/", "/sys/class/dmi/"};
  for (const auto& path : candidates) {
    std::string full_path(path);
    full_path.append("id/");
    full_path.append(name);
    std::ifstream f(full_path);
    if (f) {
      getline(f, value);
      if (!value.empty()) {
        return value;
      }
    }
  }
  return "<unknown>";
}

inline MainBoard::MainBoard() {
  _vendor = get_dmi_by_name("board_vendor");
  _name = get_dmi_by_name("board_name");
  _version = get_dmi_by_name("board_version");
  _serial_number = get_dmi_by_name("board_serial");
}

}  // namespace hwinfo

#endif  // HWINFO_UNIX
#ifdef HWINFO_UNIX

namespace hwinfo {

inline std::string getInterfaceIndex(const std::string& path) {
  int index = if_nametoindex(path.c_str());
  return (index > 0) ? std::to_string(index) : "<unknown>";
}

inline std::string getDescription(const std::string& path) { return path; }

inline std::string getMac(const std::string& path) {
  std::ifstream file("/sys/class/net/" + path + "/address");
  std::string mac;
  if (file.is_open()) {
    std::getline(file, mac);
    file.close();
  }
  return mac.empty() ? "<unknown>" : mac;
}

inline std::string getIp4(const std::string& interface) {
  std::string ip4 = "<unknown>";
  struct ifaddrs* ifaddr;
  if (getifaddrs(&ifaddr) == -1) {
    return ip4;
  }

  for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == nullptr) continue;
    if (ifa->ifa_addr->sa_family == AF_INET && interface == ifa->ifa_name) {
      char ip[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr, ip, sizeof(ip));
      ip4 = ip;
      break;
    }
  }
  freeifaddrs(ifaddr);
  return ip4;
}

inline std::string getIp6(const std::string& interface) {
  std::string ip6 = "<unknown>";
  struct ifaddrs* ifaddr;
  if (getifaddrs(&ifaddr) == -1) {
    return ip6;
  }

  for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == nullptr) {
      continue;
    }
    if (ifa->ifa_addr->sa_family == AF_INET6 && interface == ifa->ifa_name) {
      char ip[INET6_ADDRSTRLEN];
      inet_ntop(AF_INET6, &((struct sockaddr_in6*)ifa->ifa_addr)->sin6_addr, ip, sizeof(ip));
      if (std::strncmp(ip, "fe80", 4) == 0) {
        ip6 = ip;
        break;
      }
    }
  }
  freeifaddrs(ifaddr);
  return ip6;
}

inline std::vector<Network> getAllNetworks() {
  std::vector<Network> networks;
  struct ifaddrs* ifaddr;
  if (getifaddrs(&ifaddr) == -1) {
    perror("getifaddrs");
    return networks;
  }

  for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == nullptr) continue;
    if (ifa->ifa_addr->sa_family != AF_PACKET) continue;

    std::string interface = ifa->ifa_name;
    Network network;
    network._index = getInterfaceIndex(interface);
    network._description = getDescription(interface);
    network._mac = getMac(interface);
    network._ip4 = getIp4(interface);
    network._ip6 = getIp6(interface);
    networks.push_back(std::move(network));
  }
  freeifaddrs(ifaddr);
  return networks;
}

}  // namespace hwinfo

#endif

#ifdef HWINFO_UNIX

namespace hwinfo {

inline OS::OS() {
  {  // name and version
    std::string line;
    std::ifstream stream("/etc/os-release");
    if (!stream) {
      _name = "Linux";
      _version = "<unknown>";
    }
    while (std::getline(stream, line)) {
      if (utils::starts_with(line, "PRETTY_NAME")) {
        line = line.substr(line.find('=') + 1, line.length());
        // remove \" at begin and end of the substring result
        _name = {line.begin() + 1, line.end() - 1};
      }
      if (utils::starts_with(line, "VERSION=")) {
        line = line.substr(line.find('=') + 1, line.length());
        // remove \" at begin and end of the substring result
        _version = {line.begin() + 1, line.end() - 1};
      }
    }
    stream.close();
  }
  {  // Kernel
    static utsname info;
    if (uname(&info) == 0) {
      _kernel = info.release;
    } else {
      _kernel = "<unknown>";
    }
  }
  {  // architecture
    struct stat buffer{};
    _64bit = stat("/lib64/ld-linux-x86-64.so.2", &buffer) == 0;
    _32bit = !_64bit;
  }
  {  // Get endian. This is platform independent...
    char16_t dummy = 0x0102;
    _bigEndian = ((char*)&dummy)[0] == 0x01;
    _littleEndian = ((char*)&dummy)[0] == 0x02;
  }
}

}  // namespace hwinfo

#endif  // HWINFO_UNIX

#ifdef HWINFO_UNIX

namespace hwinfo {

struct MemInfo {
  uint64_t total = 0;
  uint64_t free = 0;
  uint64_t available = 0;
};

inline void get_from_sysconf(MemInfo& mi) {
  int64_t pages = sysconf(_SC_PHYS_PAGES);
  int64_t available_pages = sysconf(_SC_AVPHYS_PAGES);
  int64_t page_size = sysconf(_SC_PAGESIZE);
  if (pages > 0 && page_size > 0) {
    mi.total = pages * page_size;
  }
  if (available_pages > 0 && page_size > 0) {
    mi.available = available_pages * page_size;
  }
}

inline MemInfo parse_meminfo() {
  MemInfo mi;
  std::ifstream f_meminfo("/proc/meminfo");
  if (!f_meminfo) {
    get_from_sysconf(mi);
  } else {
    while (mi.total == 0 || mi.available == 0 || mi.free == 0) {
      std::string line;
      if (!std::getline(f_meminfo, line)) {
        if (mi.total == 0 || mi.available == 0) {
          get_from_sysconf(mi);
        }
        return mi;
      }
      auto split = utils::split(line, ":");
      if (split.size() != 2) {
        continue;
      }
      auto key = std::move(split[0]);
      auto value = std::move(split[1]);
      utils::strip(key);
      utils::strip(value);
      auto val_split = utils::split(value, " ");
      std::uint64_t val = 0;
      if (!val_split.empty()) {
        try {
          val = std::stoull(val_split[0]) * 1024;
        } catch (...) {
        }
      }
      if (key == "MemTotal") {
        mi.total = val;
      } else if (key == "MemFree") {
        mi.free = val;
      } else if (key == "MemAvailable") {
        mi.available = val;
      }
    }
  }
  return mi;
}

inline Memory::Memory() {
  // TODO: get information for actual memory modules (DIMM)
}

inline uint64_t Memory::size() const {
  auto meminfo = parse_meminfo();
  return utils::round_to_next_power_of_2(meminfo.total);
}

inline uint64_t Memory::free() const {
  auto meminfo = parse_meminfo();
  return meminfo.free;
}

inline uint64_t Memory::available() const {
  auto meminfo = parse_meminfo();
  return meminfo.available;
}

}  // namespace hwinfo

#endif  // HWINFO_UNIX
#endif
