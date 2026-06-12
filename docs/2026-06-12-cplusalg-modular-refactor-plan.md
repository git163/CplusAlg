# CplusAlg 模块化与可扩展化重构计划

## 背景与目标

当前 CplusAlg 项目已实现 C++ 通过统一接口调用 `alg/` 目录下 Python 算法模块的能力，并落地了模板匹配、erf 曲线拟合等用例。随着项目后续要不断扩充算法和后端能力，现有架构的扩展瓶颈逐渐显现：

- C++ 侧 `alg_interface.cpp` 职责混杂：Python 解释器、类型转换、传输策略、错误处理全部耦合在一个文件。
- Python 侧 `alg/core/registry.py` 使用硬编码字典，每新增一个算法需要同时改 C++ 测试和 Python 注册表。
- CMake 使用 `file(GLOB_RECURSE)`，新增源文件不会自动触发重新配置。
- 没有后端抽象，未来无法低成本接入纯 C++ 算法或远程服务。

本次重构目标：
1. **C++ 侧分层清晰**：backend / transport / type_converter / facade 分离。
2. **Python 侧自动发现**：通过 `@algorithm` 装饰器自动注册，新增算法无需改 registry。
3. **Backend 可扩展**：定义 `backend_interface`，本次实现 `PythonBackend`，并新增一个 `CppBackend` 示例验证抽象可用。
4. **CMake 模块化**：用 `add_subdirectory` 替代 GLOB，新增模块只需在对应目录放文件和 CMakeLists.txt。
5. **向后兼容**：公开 API（`cplus_alg::call` 各重载、`call_params`、`data_buffer`、`shm_handle`、日志宏）保持不变。

## 最终架构设计

### C++ 目录结构

```
src/
├── CMakeLists.txt                              # 汇总 src 下各子目录
├── main.cpp                                    # 示例入口（保持不变，内部用例可扩展）
├── include/cplus_alg/                          # 公共头文件（Google Style）
│   ├── alg_interface.h                         # 公开 API（不变）
│   ├── data_buffer.h                           # 公开 API（不变）
│   ├── shm_handle.h                            # 公开 API（不变）
│   ├── logger.h                                # 公开 API（不变，宏）
│   ├── backend/
│   │   ├── backend_interface.h                 # 后端抽象接口
│   │   └── dispatch_result.h                   # 统一返回结构
│   ├── transport/
│   │   ├── transport_strategy.h                # 传输策略接口
│   │   ├── direct_transport.h                  # 直传实现
│   │   └── shm_transport.h                     # 共享内存实现
│   └── python/
│       ├── python_backend.h                    # Python 后端实现
│       └── type_converter.h                    # nlohmann/pybind 转换
└── cplus_alg/                                  # 实现文件
    ├── CMakeLists.txt                          # core library 汇总
    ├── alg_interface.cpp                       # 薄 facade，只负责路由
    ├── backend/
    │   ├── CMakeLists.txt
    │   ├── backend_interface.cpp               # dispatch_result 辅助
    │   └── cpp_backend.cpp / .h                # 简单 C++ 后端示例
    ├── transport/
    │   ├── CMakeLists.txt
    │   ├── direct_transport.cpp
    │   └── shm_transport.cpp
    └── python/
        ├── CMakeLists.txt
        ├── python_backend.cpp
        └── type_converter.cpp
```

### Python 目录结构

```
alg/
├── __init__.py
├── core/
│   ├── __init__.py
│   ├── decorators.py         # @algorithm 装饰器
│   ├── discovery.py          # 自动扫描 alg/ 包
│   ├── registry.py           # 注册表 + dispatch（去硬编码）
│   ├── shm_io.py             # 行为不变
│   ├── mat_io.py             # 行为不变
│   └── exception.py          # 行为不变
├── template_match.py         # 加 @algorithm("template_match")
├── curve_fit.py              # 加 @algorithm("curve_fit")
└── echo.py                   # 加 @algorithm("echo")
```

### 项目资源目录结构（测试/示例）

为统一测试和 `main` 示例的输入数据、生成图像和日志的存放位置，约定固定目录结构如下：

```
CplusAlg/
├── data/                          # 测试/示例数据（样本、生成图像）
│   ├── samples/                   # 静态样本数据
│   │   └── .gitkeep
│   └── images/                    # 算法生成的图像，如 curve_fit.png
│       └── .gitkeep
├── output/                        # 编译/部署产物输出目录
├── log/                           # 运行时日志（与现有 logger.h 保持一致）
│   └── cplus_alg.log
└── scripts/
```

规则：
- `data/samples/`：存放静态样本数据，供测试和示例读取；目前测试多在代码中生成数据，可预留该目录供后续样本使用。
- `data/images/`：所有测试和示例生成的图像必须输出到此目录，禁止在项目根目录直接生成图片文件。
- `output/`：用于编译、打包、部署等产物输出，不作为运行时图像/数据存放位置。
- `log/`：继续作为主日志目录，`logger.h` 已写入 `log/cplus_alg.log`。
- 目录本身通过 `.gitkeep` 提交到仓库，运行时产生的具体文件通过 `.gitignore` 忽略。

需要同步修改的调用点：
- `tests/alg/test_curve_fit.py` 中 `plot_path` 改为 `"data/images/test_curve_fit.png"`。
- `src/main.cpp` 中 curve_fit 的 `plot_path` 改为 `"data/images/curve_fit.png"`。
- `.gitignore` 中忽略 `data/images/` 和 `log/` 下的运行时文件，但保留 `.gitkeep`。

## 关键抽象

### 1. `backend_interface`（C++）

```cpp
class backend_interface {
public:
    virtual ~backend_interface() = default;
    virtual dispatch_result dispatch(
        const std::string& module_name,
        const std::optional<data_or_handle>& input,
        const nlohmann::json& json_params,
        const std::unordered_map<std::string, data_buffer>& buffer_params) = 0;
    virtual bool available() const = 0;
};
```

- `PythonBackend`：内嵌 Python 解释器，调用 `alg.core.registry.dispatch`。
- `CppBackend`：示例实现，仅处理少数内置模块（如 `cpp_echo`），其余返回未找到。

### 2. `transport_strategy`（C++）

```cpp
class transport_strategy {
public:
    virtual ~transport_strategy() = default;
    virtual data_or_handle prepare_input(const data_buffer& buf) = 0;
};
```

- `direct_transport`：直接返回 `data_buffer` 视图。
- `shm_transport`：创建 `shm_buffer`，返回 `shm_handle`。

### 3. `@algorithm` 装饰器（Python）

```python
from alg.core.decorators import algorithm

@algorithm("template_match")
def run(input_data, params):
    ...
```

- `alg/core/discovery.py` 在 `registry` 导入时使用 `pkgutil.iter_modules` 扫描 `alg/` 下非包模块，自动 import 并触发装饰器注册。
- `registry.dispatch` 保留对未注册模块的懒加载 fallback：`importlib.import_module(f"alg.{module_name}")` 并使用 `mod.run`，确保未装饰的老模块也能工作。

## 实施步骤

### Phase 1：Python 侧注册机制与资源目录规范（低风险）

1. 创建项目资源目录结构：
   - 新建 `data/samples/`、`data/images/` 目录，并放置 `.gitkeep` 以提交空目录。
   - `output/` 作为编译/部署产物目录，本次不在其下创建 images 子目录。
   - 修改 `.gitignore`：忽略 `data/images/` 和 `log/` 下的运行时文件，但保留 `.gitkeep`（规则如 `data/images/*` + `!data/images/.gitkeep`、`log/*` + `!log/.gitkeep`）。
2. 修改图像输出路径：
   - `tests/alg/test_curve_fit.py` 的 `plot_path` 改为 `"data/images/test_curve_fit.png"`。
   - `src/main.cpp` 中 curve_fit 调用的 `plot_path` 改为 `"data/images/curve_fit.png"`。
3. 创建 `alg/core/decorators.py`，实现 `@algorithm(name)`。
4. 创建 `alg/core/discovery.py`，实现 `discover_and_register()`。
5. 修改 `alg/core/registry.py`：
   - 将硬编码 `_ALGORITHMS` 改为空 dict。
   - 新增 `register_algorithm(name, func)`。
   - `dispatch` 中新增未注册时的 lazy import fallback。
   - 模块导入时调用 `discover_and_register()`。
6. 给 `alg/template_match.py`、`alg/curve_fit.py`、`alg/echo.py` 加上装饰器。
7. 运行 Python 测试：`python -m pytest tests/alg/ -v`。

### Phase 2：C++ 类型转换提取

6. 创建 `src/include/cplus_alg/python/type_converter.h`。
7. 创建 `src/cplus_alg/python/type_converter.cpp`，将 `alg_interface.cpp` 中的 `dtype_to_str`、`buffer_to_numpy`、`buffer_to_py_input`、`shm_handle_to_py_input`、`input_to_py`、`json_to_py`、`params_to_py`、`py_to_json` 移入。
8. 修改 `src/cplus_alg/alg_interface.cpp`，包含新头文件并删除已提取的实现。
9. 配置并构建验证：`cmake -B build -S . && cmake --build build -j4 && ctest --test-dir build --output-on-failure`。

### Phase 3：C++ 传输策略提取

10. 创建 `src/include/cplus_alg/transport/transport_strategy.h`。
11. 创建 `src/include/cplus_alg/transport/direct_transport.h` 和 `src/cplus_alg/transport/direct_transport.cpp`。
12. 创建 `src/include/cplus_alg/transport/shm_transport.h` 和 `src/cplus_alg/transport/shm_transport.cpp`（将 `generate_shm_name` 移入）。
13. 修改 `alg_interface.cpp` 的 `call()` 重载，使用 `std::unique_ptr<transport_strategy>` 准备输入。
14. 构建并运行 C++ 测试。

### Phase 4：C++ Backend 抽象

15. 创建 `src/include/cplus_alg/backend/backend_interface.h` 和 `dispatch_result.h`。
16. 创建 `src/include/cplus_alg/python/python_backend.h` 和 `src/cplus_alg/python/python_backend.cpp`：
    - 将 `python_runtime` 单例和 `do_call` 移入。
    - 实现 `backend_interface`。
17. 创建 `src/include/cplus_alg/backend/cpp_backend.h` 和 `src/cplus_alg/backend/cpp_backend.cpp`：
    - 实现内置 `cpp_echo` 模块，返回输入信息和部分参数。
18. 将 `alg_interface.cpp` 改为 facade：
    - 持有全局 `std::unique_ptr<backend::backend_interface>`（默认 PythonBackend）。
    - 所有 `call()` 重载统一组装参数后调用 backend。
    - 错误处理统一在 facade 层完成。
19. 构建并运行 C++ 测试。

### Phase 5：CMake 模块化

20. 创建 `src/cplus_alg/CMakeLists.txt`：
    - `add_subdirectory(backend)`
    - `add_subdirectory(transport)`
    - `add_subdirectory(python)`
    - 汇总为 `cplus_alg_lib` 静态库。
21. 创建各子目录 `CMakeLists.txt`。
22. 修改根目录 `CMakeLists.txt`：
    - 移除 `file(GLOB_RECURSE CPLUS_ALG_SOURCES ...)`。
    - 使用 `add_subdirectory(src)` 或 `add_subdirectory(src/cplus_alg)`。
23. 修改 `tests/CMakeLists.txt`：
    - 移除 `file(GLOB_RECURSE TEST_SOURCES ...)`。
    - 显式列出测试文件，或创建 `tests/alg/CMakeLists.txt`。
24. 全量构建并运行 `./scripts/run_tests.sh`。

### Phase 6：验证扩展性

25. 新增一个临时算法 `alg/test_auto_discovery.py`（带 `@algorithm("test_auto_discovery")`），不修改 registry，从 C++ 调用它，验证自动发现可用后删除该测试文件或保留为示例。
26. 新增 C++ 测试 `TestBackendSwitch`：调用 `cpp_echo` 验证 `CppBackend` 能正常工作。

## 关键修改文件

- `CMakeLists.txt`（根）
- `src/cplus_alg/alg_interface.cpp`
- `src/cplus_alg/alg_interface.h`（保持公开 API 不变）
- `src/main.cpp`
- `alg/core/registry.py`
- `tests/CMakeLists.txt`
- `tests/alg/test_curve_fit.py`
- `.gitignore`
- 新增：
  - `alg/core/decorators.py`
  - `alg/core/discovery.py`
  - `src/include/cplus_alg/backend/*.h`
  - `src/include/cplus_alg/transport/*.h`
  - `src/include/cplus_alg/python/*.h`
  - `src/cplus_alg/backend/cpp_backend.cpp/.h`
  - `src/cplus_alg/transport/*.cpp`
  - `src/cplus_alg/python/*.cpp`
  - 各子目录 `CMakeLists.txt`
  - `data/samples/.gitkeep`
  - `data/images/.gitkeep`

## 向后兼容

- `alg_interface.h` 中的函数签名、类型、宏全部保持不变。
- Python 侧 `registry.dispatch(module_name, input_data, params)` 签名与返回格式不变。
- 未使用 `@algorithm` 装饰器的老模块仍可通过 lazy fallback 加载。
- 顶层构建产物（`cplus_alg_lib`、`CplusAlg`、`unit_tests`）名称不变。

## 验证方式

每阶段完成后执行：

```bash
cmake -B build -S . -DPython_ROOT_DIR=/opt/homebrew/Frameworks/Python.framework/Versions/3.11
cmake --build build -j4
./scripts/run_tests.sh
./build/CplusAlg
```

最终验证清单：
- CTest 所有用例通过（含新增 backend 和 transport 测试）。
- Python pytest 所有用例通过。
- `main.cpp` 正常输出模板匹配和曲线拟合结果，且 `data/images/curve_fit.png` 生成在项目根目录无散落 `.png`。
- 新增 `alg/test_auto_discovery.py` 可被 C++ 直接调用而无需修改 registry。
- `cpp_echo` 后端可通过 C++ 接口调用成功。
- 目录结构符合约定：`data/samples/`、`data/images/` 存在且通过 `.gitkeep` 保留，`log/` 下产生 `cplus_alg.log`，`output/` 用于编译/部署产物。
