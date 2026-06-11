# CplusAlg Python 算法接口设计

**日期**：2026-06-12  
**主题**：C++ 通过统一接口调用 `alg/` 目录下的 Python 算法模块  
**状态**：设计确认阶段

---

## 1. 背景与目标

CplusAlg 项目需要在 C++ 中调用 Python 图像/数值算法（OpenCV、SciPy 等）。为保持工程化与可扩展性，设计一套通用调用机制：

- Python 算法统一放在 `alg/` 目录下，按模块组织。
- C++ 通过统一入口 `alg::call` 调用任意 Python 算法。
- 支持**小数据直接传内存**、**大数据走共享内存**的混合传输策略。
- 支持未来扩展为强类型 C++ 算法封装（如 `alg::template_match`）。

本次 pilot 以**模板匹配**为首个落地算法。

---

## 2. 设计原则

1. **通用性优先**：接口不限于图像，任何“多维数组/数据块”都可通过同一机制传递。
2. **性能与通用性分层**：大数据优先性能（共享内存），小数据优先接口简洁（直传内存）。
3. **编译期类型安全**：通过 `std::variant`、`tag dispatch`、强类型封装逐层提供类型检查。
4. **RAII**：共享内存、Python 解释器等资源由 RAII 管理，避免泄漏。
5. **错误统一返回**：C++ 与 Python 之间统一使用 JSON 返回结果或错误信息。
6. **零依赖侵入**：Python 包使用系统 Python 和 pip 安装依赖，C++ 侧不内嵌 Python 环境。

---

## 3. 架构概览

```
┌─────────────────────────────────────────────────────────────┐
│  C++ Application                                            │
│  ┌──────────────┐   ┌──────────────┐   ┌─────────────────┐ │
│  │   main.cpp   │──▶│  alg::call   │──▶│ data_buffer /   │ │
│  │  typed wrap  │   │  (unified)   │   │ cv::Mat adapter │ │
│  └──────────────┘   └──────────────┘   └─────────────────┘ │
│                              │                              │
│                              ▼                              │
│                     ┌─────────────────┐                     │
│                     │  shm_handle     │                     │
│                     │  (大数据时创建)  │                     │
│                     └─────────────────┘                     │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼ pybind11 + JSON
┌─────────────────────────────────────────────────────────────┐
│  Python Package: alg/                                       │
│  ┌──────────────┐   ┌───────────────────┐   ┌────────────┐ │
│  │ core.registry│──▶│ core.shm_io       │──▶│ cv2/scipy  │ │
│  │              │   │ core.mat_io       │   │            │ │
│  └──────────────┘   └───────────────────┘   └────────────┘ │
│         │                                                   │
│         ▼                                                   │
│  ┌──────────────┐                                           │
│  │template_match│                                           │
│  └──────────────┘                                           │
└─────────────────────────────────────────────────────────────┘
```

---

## 4. 核心数据结构

### 4.1 `data_buffer`（通用数据描述）

```cpp
// src/include/cplus_alg/data_buffer.h
#pragma once
#include <vector>
#include <string>
#include <cstddef>

namespace cplus_alg {

struct data_buffer {
    std::vector<int> shape;      // 任意维度
    std::string dtype;           // "uint8", "float32", "int32", ...
    void* data = nullptr;        // 连续内存指针
    std::size_t size_bytes = 0;  // 总字节数
};

} // namespace cplus_alg
```

`data_buffer` 默认作为**非占有式视图**使用，生命周期由调用方保证。需要占有时，C++ 接口层内部使用 `std::vector<std::uint8_t>` 或 `shm_buffer` 管理。

### 4.2 `shm_handle`（共享内存句柄）

```cpp
// src/include/cplus_alg/shm_handle.h
#pragma once
#include "data_buffer.h"
#include <string>

namespace cplus_alg {

struct shm_handle {
    std::string name;            // /dev/shm 下的对象名
    std::vector<int> shape;
    std::string dtype;
    std::size_t size_bytes = 0;
};

} // namespace cplus_alg
```

### 4.3 `data_or_handle`

```cpp
// src/include/cplus_alg/alg_interface.h
#include <variant>

namespace cplus_alg {

using data_or_handle = std::variant<data_buffer, shm_handle>;

} // namespace cplus_alg
```

---

## 5. 接口设计

### 5.1 传输模式 tag

```cpp
// src/include/cplus_alg/alg_interface.h
namespace cplus_alg {

struct auto_transmit_t {};
struct direct_transmit_t {};
struct shm_transmit_t {};

inline constexpr auto_transmit_t auto_transmit{};
inline constexpr direct_transmit_t direct_transmit{};
inline constexpr shm_transmit_t shm_transmit{};

} // namespace cplus_alg
```

### 5.2 通用入口

```cpp
// src/include/cplus_alg/alg_interface.h
#include "data_buffer.h"
#include "shm_handle.h"
#include <nlohmann/json.hpp>

namespace cplus_alg {

// 1. 自动按大小选择传输方式
nlohmann::json call(
    const std::string& module_name,
    const data_or_handle& input,
    const nlohmann::json& params = {});

// 2. 强制直接传内存
nlohmann::json call(
    direct_transmit_t,
    const std::string& module_name,
    const data_buffer& input,
    const nlohmann::json& params = {});

// 3. 强制走共享内存
nlohmann::json call(
    shm_transmit_t,
    const std::string& module_name,
    const data_buffer& input,
    const nlohmann::json& params = {});

// 4. 无数据输入，纯参数调用
nlohmann::json call(
    const std::string& module_name,
    const nlohmann::json& params = {});

} // namespace cplus_alg
```

### 5.3 tag dispatch 说明

- `direct_transmit` 只接受 `data_buffer`，不接受 `shm_handle`。
- `shm_transmit` 只接受 `data_buffer`（由 C++ 侧创建 shm），不接受已存在的 `shm_handle`。
- 调用方传错类型时，在编译期即可暴露。

### 5.4 适配器

```cpp
// src/include/cplus_alg/data_adapters/cv_mat_adapter.h
#include "cplus_alg/data_buffer.h"
#include <opencv2/core.hpp>

namespace cplus_alg {

data_buffer from_cv_mat(const cv::Mat& mat);

cv::Mat to_cv_mat(const data_buffer& buf);

} // namespace cplus_alg
```

### 5.5 强类型封装层（第二阶段）

```cpp
// src/include/cplus_alg/template_match.h
#pragma once
#include "alg_interface.h"

namespace cplus_alg {

enum class match_method {
    sqdiff,
    sqdiff_normed,
    ccorr,
    ccorr_normed,
    ccoef,
    ccoef_normed
};

struct match_result {
    int x = 0;
    int y = 0;
    double score = 0.0;
};

match_result template_match(
    const data_or_handle& image,
    const data_buffer& templ,
    match_method method = match_method::ccorr_normed,
    const std::optional<data_buffer>& mask = std::nullopt);

} // namespace cplus_alg
```

---

## 6. 数据流

### 6.1 模板匹配（大图走 shm）

1. C++ 加载 `cv::Mat image`。
2. 判断大小：超过阈值（默认 1MB）则创建共享内存。
3. `alg::call("template_match", image_handle_or_buffer, params)`。
4. Python `core.registry` 路由到 `template_match.run`。
5. `core.shm_io.read_image(input_data)` 读取 numpy array。
6. 执行 `cv2.matchTemplate`。
7. 返回 `{"success": true, "data": {"x", "y", "score"}}`。
8. C++ 释放共享内存。

### 6.2 小图/小数据

1. `data_buffer` 直接通过 pybind11 转成 numpy array。
2. 无需创建 shm。
3. 结果直接返回在 JSON 中。

---

## 7. 共享内存生命周期

### 7.1 创建

- C++ 生成 UUID 作为 shm 名，如 `cplusalg_img_a1b2c3d4`。
- 调用 `shm_open()` + `ftruncate()` 创建指定大小段。
- 将 `data_buffer` 数据 `memcpy` 到 shm。

### 7.2 使用

- Python 通过 `posix_ipc.SharedMemory(name)` 附加同一段。
- 使用 `numpy.frombuffer` + `reshape` 读取，零拷贝视图。

### 7.3 释放

- C++ 侧使用 RAII 类 `shm_buffer` 管理。
- 析构时自动 `munmap()` + `shm_unlink()`。
- 即使 Python 异常或 C++ 异常，也能保证清理。

```cpp
// src/include/cplus_alg/shm_handle.h
class shm_buffer {
public:
    shm_buffer(const std::string& name, std::size_t size);
    ~shm_buffer() noexcept;

    void* data() const noexcept;
    std::size_t size() const noexcept;
    std::string name() const;

    shm_buffer(const shm_buffer&) = delete;
    shm_buffer& operator=(const shm_buffer&) = delete;
    shm_buffer(shm_buffer&&) noexcept;
    shm_buffer& operator=(shm_buffer&&) noexcept;

private:
    std::string name_;
    void* addr_ = nullptr;
    std::size_t size_ = 0;
};
```

---

## 8. Python 包结构

```
alg/
├── __init__.py
├── core/
│   ├── __init__.py
│   ├── shm_io.py          # 共享内存读写
│   ├── mat_io.py          # 小数据直接内存转换
│   ├── exception.py       # 异常统一封装
│   └── registry.py        # 算法路由
├── template_match.py      # 模板匹配
├── image_filter.py        # 滤波/形态学（预留）
└── image_transform.py     # 几何变换（预留）
```

### 8.1 算法模块约定

每个算法模块必须实现 `run(input_data, params) -> dict`：

```python
def run(input_data, params):
    """
    input_data:
        - None: 无数据输入
        - {"type": "buffer", "array": np.ndarray, "shape": [...], "dtype": "..."}
        - {"type": "shm", "handle": {"name": "...", "shape": [...], "dtype": "...", "size_bytes": N}}
    params: dict，算法参数
    return: {"success": bool, "data": ...} 或 {"success": False, "error": str, "error_type": str}
    """
```

### 8.2 注册表

```python
# alg/core/registry.py
_ALGORITHMS = {
    "template_match": "alg.template_match",
}

def dispatch(module_name: str, input_data, params: dict) -> dict:
    if module_name not in _ALGORITHMS:
        return {"success": False, "error": f"unknown module: {module_name}"}
    try:
        mod = importlib.import_module(_ALGORITHMS[module_name])
        return mod.run(input_data, params)
    except Exception as e:
        return {"success": False, "error": str(e), "error_type": type(e).__name__}
```

---

## 9. 错误处理

### 9.1 统一返回结构

成功：
```json
{
  "success": true,
  "data": { ... }
}
```

失败：
```json
{
  "success": false,
  "error": "错误描述",
  "error_type": "PythonException"
}
```

### 9.2 错误来源与处理

| 来源 | 处理方式 |
|---|---|
| C++ 异常 | 直接抛出 `std::runtime_error` |
| Python 异常 | `pybind11` 捕获并转成 JSON error |
| 算法返回格式错误 | C++ 层校验后返回 error JSON |
| shm 创建失败 | 抛异常或返回 error JSON |
| 模块不存在 | 返回 `{"success": false, "error": "module not found"}` |
| cv2/scipy 运行时错误 | Python 捕获并返回 error JSON |

### 9.3 异常安全

- `shm_buffer` 析构自动清理。
- Python 解释器由单例 `scoped_interpreter` 管理，程序退出自动关闭。
- 调用失败不导致进程崩溃。

---

## 10. 测试策略

### 10.1 Python 单元测试

- 文件：`tests/alg/test_template_match.py`
- 工具：`pytest`
- 覆盖：算法正确性、参数解析、错误返回格式。

### 10.2 Python 集成测试

- 文件：`tests/alg/test_shm_io.py`
- 覆盖：shm round-trip、大小图分支。

### 10.3 C++ 单元测试

- 文件：`tests/alg/TestAlgInterface.cpp`
- 工具：GoogleTest
- 覆盖：`data_buffer` 构造、`cv::Mat` 适配器、`shm_buffer` RAII、JSON 解析。

### 10.4 C++ 集成测试

- 文件：`tests/alg/TestTemplateMatch.cpp`
- 覆盖：真实模板匹配调用、大图 shm 路径、小图直传路径、错误路径。

### 10.5 端到端测试

- 构造合成图像。
- C++ 加载 → 调用 Python 算法 → 验证结果。

---

## 11. 依赖

### 11.1 C++ 依赖

- CMake ≥ 3.20
- C++17 编译器
- Python 开发库
- pybind11（已集成到 `third_party/`）
- nlohmann/json（已集成到 `third_party/`）
- OpenCV C++（用于 `cv::Mat` 适配器，可选）

### 11.2 Python 依赖

用户需自行安装：

```bash
pip install opencv-python numpy scipy posix_ipc
```

声明在 `requirements.txt` 中。

---

## 12. 风险与后续

### 12.1 风险

1. **跨平台共享内存**：`posix_ipc` 在 Linux/macOS 可用，Windows 需替换方案。
2. **GIL 开销**：高频调用时 Python GIL 可能成为瓶颈。
3. **OpenCV 版本差异**：C++ `cv::Mat` 与 Python `cv2` 的 dtype/channel 约定需对齐。

### 12.2 性能优化路径

若接口调用延迟成为瓶颈，按以下顺序优化：

1. **减少单次调用开销**
   - C++ 启动时预导入常用 Python 模块。
   - Python 解释器常驻，避免重复启动。
   - 批量调用：一次传入多张图，Python 内部向量化处理。

2. **共享内存池**
   - 用预先分配的 shm 段循环使用，减少反复 `shm_open` / `shm_unlink` 的系统调用开销。

3. **多进程并行**
   - 每个进程拥有独立 Python 解释器和 GIL，绕过 GIL 限制。
   - 数据通过 shm 传递，降低进程间序列化成本。
   - 适合无状态、可并行的任务。

4. **多线程（有限）**
   - 仅当 Python 算法以 IO 等待为主时有效。
   - CPU 密集型 Python 代码受 GIL 限制，多线程无法真正并行。

5. **热点迁移回 C++**
   - 通过 profiling 定位瓶颈。
   - 把热点算法或其中耗时部分用 C++ 实现，再通过 pybind11 暴露给 Python，或直接替换 Python 实现。

### 12.3 后续扩展

1. **shm 池**：如果 pilot 性能不足，升级到可复用的共享内存池。
2. **更多适配器**：如 `Eigen::Matrix`、`std::vector` 的适配器。
3. **异步/流水线调用**：支持一次调用处理多帧或多张图。
4. **强类型封装层**：稳定后逐步为高频算法提供强类型 C++ 函数。
5. **跨平台共享内存**：Windows 下替换 `posix_ipc` 方案。

---

## 13. 附录

### 13.1 大小阈值默认值

```cpp
constexpr std::size_t k_small_data_threshold_bytes = 1 * 1024 * 1024; // 1MB
```

### 13.2 命名约定

- C++ 文件：`underscore_style.h` / `.cpp`
- 类/函数：`underscore_style`
- 成员变量：`trailing_`
- Python 模块：`snake_case.py`

### 13.3 参考资料

- pybind11 嵌入 Python 文档
- POSIX 共享内存（`shm_open` / `shm_unlink`）
- `numpy.frombuffer` 零拷贝视图
