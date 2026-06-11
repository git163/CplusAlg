# CplusAlg Python 算法接口实现计划

- 日期: 2026-06-12
- 作者: Claude
- 状态: 已批准
- 关联设计: `docs/superpowers/specs/2026-06-12-alg-interface-design.md`

## 背景

设计文档已确认，需要实现 C++ 调用 `alg/` 目录下 Python 算法模块的端到端能力。首个落地算法为模板匹配。

## 目标

1. 实现 `alg/` Python 包，支持模板匹配。
2. 实现 C++ 统一调用入口 `cplus_alg::call`。
3. 支持小数据直传、大数据共享内存传输。
4. 提供 C++ 和 Python 测试，验证端到端流程。

### 非目标

- 不强类型封装层（第二阶段）。
- 不实现 shm 池、多进程、异步流水线等优化（后续按需扩展）。
- 不处理 Windows 平台共享内存。

## 方案

按以下顺序实现：

1. Python 包骨架
2. Python 核心模块（注册表、shm_io、mat_io）
3. C++ 数据结构（data_buffer、shm_handle）
4. C++ 共享内存 RAII（shm_buffer）
5. C++ 接口层（alg_interface）与 cv::Mat 适配器
6. 测试与验证

## 实施步骤

- [x] 创建实现计划文档
- [ ] 创建 `alg/` Python 包结构
  - [ ] `alg/__init__.py`
  - [ ] `alg/core/__init__.py`
  - [ ] `alg/template_match.py`
- [ ] 实现 Python 核心模块
  - [ ] `alg/core/exception.py`
  - [ ] `alg/core/registry.py`
  - [ ] `alg/core/mat_io.py`
  - [ ] `alg/core/shm_io.py`
- [ ] 实现 C++ 核心头文件
  - [ ] `src/include/cplus_alg/data_buffer.h`
  - [ ] `src/include/cplus_alg/shm_handle.h`
  - [ ] `src/include/cplus_alg/alg_interface.h`
  - [ ] `src/include/cplus_alg/data_adapters/cv_mat_adapter.h`
- [ ] 实现 C++ 源文件
  - [ ] `src/cplus_alg/shm_buffer.cpp`（RAII 共享内存）
  - [ ] `src/cplus_alg/alg_interface.cpp`
  - [ ] `src/cplus_alg/data_adapters/cv_mat_adapter.cpp`
- [ ] 更新 `src/main.cpp` 为模板匹配示例
- [ ] 更新 `CMakeLists.txt` 添加新源文件和 OpenCV 检测
- [ ] 编写测试
  - [ ] `tests/alg/test_template_match.py`
  - [ ] `tests/alg/test_shm_io.py`
  - [ ] `tests/alg/TestAlgInterface.cpp`
  - [ ] `tests/alg/TestTemplateMatch.cpp`
- [ ] 构建并验证
  - [ ] `cmake --build build`
  - [ ] `ctest --test-dir build --output-on-failure`
  - [ ] 运行 `build/CplusAlg`

## 测试计划

1. Python 单元测试：验证 `template_match.run` 对 buffer 输入返回正确坐标。
2. Python 集成测试：验证 `shm_io.read_image` 能从 shm 正确还原 numpy array。
3. C++ 单元测试：验证 `shm_buffer` RAII、cv::Mat 适配器。
4. C++ 集成测试：验证 `cplus_alg::call("template_match", ...)` 端到端成功。

## 风险与对策

| 风险 | 对策 |
|---|---|
| OpenCV 未安装导致构建失败 | CMake 中 OpenCV 设为可选，未找到时跳过 cv::Mat 适配器 |
| `posix_ipc` 未安装 | Python 测试跳过，文档中说明依赖 |
| 共享内存权限不足 | 使用 `/dev/shm/` 默认路径，测试使用当前用户权限 |
| GIL / 多线程问题 | pilot 阶段不引入多线程 |

## 引用

- `docs/superpowers/specs/2026-06-12-alg-interface-design.md`
- pybind11 文档
- `posix_ipc` 文档
