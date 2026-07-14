# hwinfo

`hwinfo` 是一个 C++17 单头文件硬件信息库，用于一次性读取 CPU、GPU、操作系统、内存、磁盘、主板和网络设备信息。

库文件只有一个：

```cpp
#include <hwinfo/hwinfo.h>
```

## 支持平台

| 组件 | Windows | Linux | macOS |
| --- | --- | --- | --- |
| CPU | 支持 | 支持 | 支持 |
| GPU | 支持 | 支持 | 暂未实现 |
| OS | 支持 | 支持 | 支持 |
| Memory | 支持 | 支持 | 支持 |
| Disk | 支持 | 支持 | 支持 |
| MainBoard | 支持 | 支持 | 支持 |
| Network | 支持 | 支持 | 暂未实现 |

部分扩展字段依赖操作系统是否提供对应数据。无法取得字符串时通常返回空字符串或 `"<unknown>"`，数值通常返回 `0`。

## 快速编译 examples

要求本机已安装 CMake 3.22 或更高版本，以及支持 C++17 的编译器。

Windows：

```bat
build.bat
```

Linux 或 macOS：

```sh
./build.sh
```

编译结果统一位于：

```text
build/bin/system_info
build/bin/hardware_summary
```

Windows 下文件名带有 `.exe` 后缀。

## CMake 接入

将本仓库作为子目录加入项目：

```cmake
add_subdirectory(path/to/hwinfo)

add_executable(my_program main.cpp)
target_link_libraries(my_program PRIVATE hwinfo)
```

`hwinfo` 是 `INTERFACE` target，不会生成静态库或动态库。CMake target 负责传递头文件路径、C++17 要求以及 Windows/macOS 所需的系统链接依赖。

## CPU

使用 `hwinfo::getAllCPUs()` 获取 CPU socket 列表：

```cpp
#include <hwinfo/hwinfo.h>

#include <iostream>

int main() {
  for (const auto& cpu : hwinfo::getAllCPUs()) {
    std::cout << "CPU " << cpu.id() << '\n'
              << "  vendor: " << cpu.vendor() << '\n'
              << "  model: " << cpu.modelName() << '\n'
              << "  physical cores: " << cpu.numPhysicalCores() << '\n'
              << "  logical cores: " << cpu.numLogicalCores() << '\n';
  }
}
```

主要接口：

| 接口 | 含义 |
| --- | --- |
| `id()` | CPU socket ID |
| `vendor()` | CPU 厂商 |
| `modelName()` | CPU 型号名称 |
| `numPhysicalCores()` | 物理核心数 |
| `numLogicalCores()` | 逻辑核心数 |
| `flags()` | CPU feature flags，平台支持程度不同 |
| `cores()` | 每核心 cache、频率和 SMT 信息，平台支持程度不同 |

## GPU

使用 `hwinfo::getAllGPUs()` 获取物理 GPU 列表：

```cpp
for (const auto& gpu : hwinfo::getAllGPUs()) {
  std::cout << "GPU " << gpu.id() << '\n'
            << "  vendor: " << gpu.vendor() << '\n'
            << "  model: " << gpu.name() << '\n'
            << "  vendor ID: " << gpu.vendor_id() << '\n'
            << "  device ID: " << gpu.device_id() << '\n'
            << "  dedicated memory: " << gpu.dedicated_memory_Bytes() << " bytes\n"
            << "  shared memory: " << gpu.shared_memory_Bytes() << " bytes\n"
            << "  frequency: " << gpu.frequency_hz() << " Hz\n";
}
```

Windows 实现会过滤软件显示设备和虚拟显示适配器。Linux 通过 `/sys/class/drm` 读取 GPU，并尝试使用系统的 `pci.ids` 解析型号。macOS 的 GPU 枚举目前返回空列表。

显存与频率等扩展字段并非所有平台都能提供；不支持时返回 `0`。

## 操作系统

构造 `hwinfo::OS` 即可取得当前操作系统快照：

```cpp
hwinfo::OS os;

std::cout << "name: " << os.name() << '\n'
          << "version: " << os.version() << '\n'
          << "kernel: " << os.kernel() << '\n'
          << "64 bit: " << (os.is64bit() ? "yes" : "no") << '\n'
          << "little endian: " << (os.isLittleEndian() ? "yes" : "no") << '\n';
```

可用接口：

- `name()`：操作系统名称。
- `version()`：操作系统版本。
- `kernel()`：内核或系统 build 信息。
- `is32bit()` / `is64bit()`：系统位数。
- `isBigEndian()` / `isLittleEndian()`：字节序。

## 内存

构造 `hwinfo::Memory` 获取内存信息：

```cpp
hwinfo::Memory memory;

std::cout << "total: " << memory.size() << " bytes\n"
          << "free: " << memory.free() << " bytes\n"
          << "available: " << memory.available() << " bytes\n";
```

接口含义：

| 接口 | 含义 |
| --- | --- |
| `size()` | RAM 总量，单位 bytes |
| `free()` | 当前空闲内存，单位 bytes |
| `available()` | 当前可用内存，单位 bytes |
| `modules()` | 内存条列表，目前主要由 Windows 提供 |

`modules()` 中的模块包含 ID、厂商、名称、型号、序列号、容量和频率。Linux/macOS 当前可能返回空模块列表，但 `size()` 仍可正常取得系统 RAM 总量。

## 磁盘

使用 `hwinfo::getAllDisks()` 获取物理磁盘：

```cpp
for (const auto& disk : hwinfo::getAllDisks()) {
  std::cout << "Disk " << disk.id() << '\n'
            << "  vendor: " << disk.vendor() << '\n'
            << "  model: " << disk.model() << '\n'
            << "  serial: " << disk.serial_number() << '\n'
            << "  size: " << disk.size() << " bytes\n"
            << "  interface: " << disk.disk_interface() << '\n'
            << "  solid state: " << (disk.is_solid_state() ? "yes" : "no") << '\n';
}
```

`disk_interface()` 返回 `hwinfo::Disk::Interface`，可能的类型包括 NVMe、SATA、SCSI、USB 以及不同速率的 USB 版本。

`is_solid_state()` 的规则：

- `true`：检测为固态存储。
- `false`：检测为机械存储，或者操作系统未能判断介质类型。

Windows 使用 seek-penalty 属性判断，Linux 读取 sysfs 的 `queue/rotational`，macOS 查询 I/O Registry。

`mount_points()` 返回与磁盘关联的卷或设备路径，其具体格式与操作系统有关。

## 主板

构造 `hwinfo::MainBoard` 获取主板或平台信息：

```cpp
hwinfo::MainBoard board;

std::cout << "vendor: " << board.vendor() << '\n'
          << "name: " << board.name() << '\n'
          << "version: " << board.version() << '\n'
          << "serial: " << board.serialNumber() << '\n';
```

Windows 使用 WMI，Linux 读取 DMI sysfs，macOS 查询 I/O Registry。部分整机厂商可能不会提供完整的型号、版本或序列号。

## 网络设备

使用 `hwinfo::getAllNetworks()` 获取网络接口：

```cpp
for (const auto& network : hwinfo::getAllNetworks()) {
  std::cout << "interface: " << network.interfaceIndex() << '\n'
            << "  description: " << network.description() << '\n'
            << "  MAC: " << network.mac() << '\n'
            << "  IPv4: " << network.ip4() << '\n'
            << "  IPv6: " << network.ip6() << '\n';
}
```

Windows 和 Linux 已实现网络接口枚举。macOS 当前返回空列表。同一个网络接口可能同时具有 IPv4 和 IPv6 地址。

## 单位换算

`hwinfo::unit` 提供 SI 和 IEC 单位换算：

```cpp
const hwinfo::Memory memory;

double gib = hwinfo::unit::unit_prefix_to(memory.size(), hwinfo::unit::IECPrefix::GIBI);
double mhz = hwinfo::unit::unit_prefix_to(3'600'000'000ULL, hwinfo::unit::SiPrefix::MEGA);
```

常用 IEC 单位包括 `KIBI`、`MEBI`、`GIBI`；常用 SI 单位包括 `KILO`、`MEGA`、`GIGA`。

## 完整示例

- [`examples/hardware_summaryMain.cpp`](examples/hardware_summaryMain.cpp)：只读取 CPU 型号与核心数、GPU 型号、RAM 总量、磁盘型号和 SSD 状态。
- [`examples/system_infoMain.cpp`](examples/system_infoMain.cpp)：展示所有当前公开组件和扩展字段。
