# hwinfo

`hwinfo` 是一个 C++17 单头文件硬件信息库，用于一次性读取 CPU、GPU、操作系统、内存、磁盘、主板和网络设备信息。
基础采集 API 仍然只需要一个头文件：

```cpp
#include <hwinfo/hwinfo.h>
```

可选的硬件需求评估层使用：

```cpp
#include <hwinfo/requirements.h>
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
build/bin/hardware_requirements
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

需要区分机械存储和检测失败时，使用 `media_type()`：

```cpp
switch (disk.media_type()) {
  case hwinfo::Disk::MediaType::SOLID_STATE:
    break;
  case hwinfo::Disk::MediaType::ROTATIONAL:
    break;
  case hwinfo::Disk::MediaType::UNKNOWN:
    break;
}
```

Windows 使用 seek-penalty 属性判断，Linux 读取 sysfs 的 `queue/rotational`，macOS 查询 I/O Registry。

`mount_points()` 返回与磁盘关联的卷或设备路径，其具体格式与操作系统有关。

## 硬件需求评估

可选的 `requirements.h` 上层接口可以按 CPU 物理核心总数、GPU 性能目录、RAM 总量和是否存在 SSD
评估当前机器。结果使用 `PASSED`、`FAILED` 和 `UNKNOWN` 三态；无法采集或无法识别的设备不会被误判为不满足。

```cpp
#include <hwinfo/requirements.h>

hwinfo::requirements::GpuCatalog gpu_catalog;
std::string error;
if (!gpu_catalog.loadCsvFile("gpu_catalog.csv", &error)) {
  // handle error
}

hwinfo::requirements::MachineRequirements requirement;
requirement.min_physical_cpu_cores = 4;
requirement.min_gpu_model = "GeForce GTX 1060";
requirement.min_memory_bytes = 8ull * 1024ull * 1024ull * 1024ull;
requirement.require_solid_state_disk = true;

const auto report = hwinfo::requirements::evaluateCurrentMachine(requirement, gpu_catalog);
if (report.overall == hwinfo::requirements::EvaluationStatus::PASSED) {
  // all enabled requirements are satisfied
}
```

默认聚合规则：

- CPU：累加所有 socket 的物理核心数。
- GPU：任意一个成功识别的物理 GPU 达到要求即通过。
- RAM：比较系统总内存。
- Disk：存在任意一个 SSD 即通过。
- 任一明确不满足时总体为 `FAILED`；没有失败但存在无法判断的项目时总体为 `UNKNOWN`。

评估逻辑也可以作用于人工构造的 `HardwareSnapshot`，不读取当前机器，便于自动化测试或评估远程采集结果：

```cpp
const auto report = hwinfo::requirements::evaluate(snapshot, requirement, gpu_catalog);
```

### GPU 目录格式

GPU 目录是带表头的 CSV，必需字段为 `canonical_model` 和正整数 `score`。可选字段包括 `rank`、`vendor`、
`aliases`、`vendor_id` 和 `device_id`。别名之间使用 `|` 分隔。比较使用分数而不是排名，因此同一份目录中分数越高表示性能越高。

```csv
canonical_model,score,rank,vendor,aliases,vendor_id,device_id
GeForce GTX 1060,100,2,NVIDIA,GTX 1060,,
GeForce RTX 3060,200,1,NVIDIA,,,
```

仓库提供 `tools/update_passmark_gpu_catalog.py`，可以为已阅读并接受 [PassMark 使用条款](https://www.passmark.com/legal/disclaimer.php) 的用户建立本地
PassMark G3D 快照：

```sh
python tools/update_passmark_gpu_catalog.py --acknowledge-terms
```

生成的原始页面和 CSV 位于 `data/local/`。该目录已被 Git 忽略；PassMark 数据受版权保护，不应在未获得
授权的情况下提交或再分发。核心代码和测试不依赖该数据。

### 将目录和要求固化到本地二进制

默认关闭的 `HWINFO_BUILD_LOCAL_REQUIREMENTS` 选项会在 CMake 配置阶段读取本地 CSV，并将目录内容和
四项要求写入 `hardware_requirements_embedded`。生成的程序运行时不读取 GPU CSV：

Windows 可以直接运行已经固化默认门槛的脚本：

```bat
build_embed.bat
```

Linux、macOS 或 Git Bash 使用：

```sh
./build_embed.sh
```

Windows 输出为 `build/bin/hardware_requirements_embedded.exe`，其他平台输出为
`build/bin/hardware_requirements_embedded`。脚本会在配置前检查本地 PassMark CSV 是否存在。

也可以通过 CMake 手动指定门槛：

```sh
cmake -S . -B build \
  -DHWINFO_BUILD_LOCAL_REQUIREMENTS=ON \
  -DHWINFO_LOCAL_MIN_CPU_CORES=4 \
  -DHWINFO_LOCAL_MIN_GPU_MODEL="GeForce RTX 3080 Ti" \
  -DHWINFO_LOCAL_MIN_MEMORY_GIB=8 \
  -DHWINFO_LOCAL_REQUIRE_SSD=ON
cmake --build build --config Release --target hardware_requirements_embedded
```

运行：

```text
build/bin/hardware_requirements_embedded
```

可用的本地配置项：

| CMake 配置项 | 默认值 | 含义 |
| --- | --- | --- |
| `HWINFO_LOCAL_GPU_CATALOG` | `data/local/passmark_gpu_catalog.csv` | 配置阶段读取的 GPU CSV |
| `HWINFO_LOCAL_MIN_CPU_CORES` | `4` | 最少物理 CPU 核心总数 |
| `HWINFO_LOCAL_MIN_GPU_MODEL` | `GeForce RTX 3080 Ti` | 最低 GPU 型号 |
| `HWINFO_LOCAL_MIN_MEMORY_GIB` | `8` | 最少 RAM，单位 GiB |
| `HWINFO_LOCAL_REQUIRE_SSD` | `ON` | 是否要求至少一个 SSD |

更新 CSV 或要求后必须重新运行 CMake 配置并重新编译。编译完成后修改或删除外部 CSV 不会改变检测结果。
这可以防止通过替换外部指标文件修改门槛，但不能防止直接篡改或替换可执行文件；有对抗性防篡改需求时，
还应对最终程序进行代码签名或校验。嵌入后的程序本身包含 PassMark 数据，未取得授权时也只能本地使用，
不能随仓库或发布包分发。

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
- [`examples/embeddedRequirementsMain.cpp`](examples/embeddedRequirementsMain.cpp)：运行目录和门槛都已固化的本地检测程序。
- [`examples/requirementsMain.cpp`](examples/requirementsMain.cpp)：加载 GPU CSV 目录并评估当前机器是否达到示例门槛。
- [`examples/system_infoMain.cpp`](examples/system_infoMain.cpp)：展示所有当前公开组件和扩展字段。
