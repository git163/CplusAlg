#pragma once

#include <pybind11/embed.h>

namespace py = pybind11;

// GIL 获取 RAII 工具
// 在 PYBIND11_SIMPLE_GIL_MANAGEMENT 模式下使用 PyGILState_Ensure/Release，多线程安全
class GilScopedAcquire {
public:
    GilScopedAcquire() = default;

    // 禁止拷贝和移动
    GilScopedAcquire(const GilScopedAcquire&) = delete;
    GilScopedAcquire& operator=(const GilScopedAcquire&) = delete;
    GilScopedAcquire(GilScopedAcquire&&) = delete;
    GilScopedAcquire& operator=(GilScopedAcquire&&) = delete;

private:
    py::gil_scoped_acquire m_acquire;
};

// GIL 释放 RAII 工具
// 在 PYBIND11_SIMPLE_GIL_MANAGEMENT 模式下使用 PyEval_SaveThread/RestoreThread
class GilScopedRelease {
public:
    GilScopedRelease()
        : m_bReleased(false), m_tstate(nullptr) {
        m_tstate = PyEval_SaveThread();
        m_bReleased = true;
    }

    ~GilScopedRelease() {
        if (m_bReleased && m_tstate) {
            PyEval_RestoreThread(m_tstate);
        }
    }

    // 禁止拷贝和移动
    GilScopedRelease(const GilScopedRelease&) = delete;
    GilScopedRelease& operator=(const GilScopedRelease&) = delete;
    GilScopedRelease(GilScopedRelease&&) = delete;
    GilScopedRelease& operator=(GilScopedRelease&&) = delete;

private:
    bool m_bReleased;
    PyThreadState* m_tstate;
};
