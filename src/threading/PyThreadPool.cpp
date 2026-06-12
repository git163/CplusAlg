#include "PyThreadPool.h"
#include <spdlog/spdlog.h>

PyThreadPool::PyThreadPool(size_t nThreads, size_t nMaxQueueSize)
    : m_nMaxQueueSize(nMaxQueueSize) {
    assert(nThreads > 0 && "nThreads must be positive");
    for (size_t i = 0; i < nThreads; ++i) {
        m_vecWorkers.emplace_back([this, i]() {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_cv.wait(
                        lock, [this] { return m_bStop || !m_tasks.empty(); });
                    if (m_bStop && m_tasks.empty()) {
                        break;
                    }
                    task = std::move(m_tasks.front());
                    m_tasks.pop();
                }
                m_cvProduce.notify_one();
                GilScopedAcquire gil;
                try {
                    task();
                } catch (const std::exception& e) {
                    spdlog::error("PyThreadPool worker {} task failed: {}", i,
                                  e.what());
                }
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    --m_nPendingTasks;
                }
                m_cvDone.notify_all();
            }
        });
    }
    spdlog::info("PyThreadPool created with {} workers, maxQueueSize={}",
                 nThreads, nMaxQueueSize);
}

PyThreadPool::~PyThreadPool() {
    Shutdown();
}

void PyThreadPool::Shutdown() {
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_bStop) {
            return;
        }
        m_bStop = true;
    }
    m_cv.notify_all();
    m_cvProduce.notify_all();
    for (auto& worker : m_vecWorkers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    spdlog::info("PyThreadPool shutdown complete");
}

void PyThreadPool::WaitAll() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cvDone.wait(lock,
                  [this] { return m_nPendingTasks.load() == 0; });
}
