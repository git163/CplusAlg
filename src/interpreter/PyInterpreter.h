#pragma once

#include <pybind11/embed.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace py = pybind11;

// Python 解释器单例管理类
// 负责解释器的初始化、sys.path 配置和关闭
// 整个进程只有一个解释器实例
class PyInterpreter {
public:
    static PyInterpreter& Instance();

    // 初始化 Python 解释器，支持传入额外的模块搜索路径
    // 幂等：多次调用只有第一次生效
    bool Initialize(const std::vector<std::string>& vecExtraPaths = {});

    // 检查解释器是否已初始化
    bool IsInitialized() const;

    // 关闭 Python 解释器
    // 注意：Finalize 后可通过 Initialize 重新初始化
    void Finalize();

    // 禁止拷贝和移动
    PyInterpreter(const PyInterpreter&) = delete;
    PyInterpreter& operator=(const PyInterpreter&) = delete;
    PyInterpreter(PyInterpreter&&) = delete;
    PyInterpreter& operator=(PyInterpreter&&) = delete;

private:
    PyInterpreter() = default;
    ~PyInterpreter();

    void SetupSysPath(const std::vector<std::string>& vecExtraPaths,
                      bool b_append_default);

    std::atomic<bool> m_bInitialized{false};
    std::mutex m_mutex;
};
