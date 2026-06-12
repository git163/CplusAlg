#pragma once

#include "interpreter/GilManager.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <spdlog/spdlog.h>
#include <thread>
#include <vector>

// Python 多线程生产者-消费者模型
// 专用于 C++ 多线程环境下通过 pybind11 调用 Python
// 消费者线程内部自动 GilScopedAcquire / GilScopedRelease
// 使用方式：
//   PyProducerConsumer<TaskData> pc(4, [](const TaskData& data) {
//       // 此处已在 GIL 保护下
//       PyModule caller("demo.numpy_ops");
//       caller.Call("stats_array", data.arr);
//   }, 100);
//   pc.Produce(std::move(data));
//   pc.WaitAll();
//
// 也支持直接绑定类成员函数：
//   MyProcessor processor;
//   PyProducerConsumer<int> pc(4, &processor, &MyProcessor::Handle, 100);
//   pc.Produce(42);
template <typename T>
class PyProducerConsumer {
public:
    using ConsumerFunc = std::function<void(const T&)>;

    explicit PyProducerConsumer(size_t nConsumers,
                                ConsumerFunc func,
                                size_t nMaxQueueSize = 0)
        : m_funcConsumer(std::move(func)),
          m_nMaxQueueSize(nMaxQueueSize),
          m_bStop(false),
          m_nPendingTasks(0),
          m_nProcessedCount(0),
          m_nProducedCount(0) {
        for (size_t i = 0; i < nConsumers; ++i) {
            m_vecConsumers.emplace_back([this, i]() {
                ConsumerLoop(i);
            });
        }
        spdlog::info("PyProducerConsumer created with {} consumers, maxQueueSize={}",
                     nConsumers, nMaxQueueSize);
    }

    // 支持类成员函数（非 const）
    template <typename ClassType>
    PyProducerConsumer(size_t nConsumers,
                       ClassType* pObj,
                       void (ClassType::*pMemFunc)(const T&),
                       size_t nMaxQueueSize = 0)
        : PyProducerConsumer(nConsumers,
                             [pObj, pMemFunc](const T& val) {
                                 (pObj->*pMemFunc)(val);
                             },
                             nMaxQueueSize) {}

    // 支持类成员函数（const）
    template <typename ClassType>
    PyProducerConsumer(size_t nConsumers,
                       const ClassType* pObj,
                       void (ClassType::*pMemFunc)(const T&) const,
                       size_t nMaxQueueSize = 0)
        : PyProducerConsumer(nConsumers,
                             [pObj, pMemFunc](const T& val) {
                                 (pObj->*pMemFunc)(val);
                             },
                             nMaxQueueSize) {}

    ~PyProducerConsumer() {
        Shutdown();
    }

    // 完美转发生产，直接在队列中构造对象
    template <typename... Args>
    void Produce(Args&&... args) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_bStop) {
                throw std::runtime_error(
                    "Cannot produce to stopped PyProducerConsumer");
            }
            if (m_nMaxQueueSize > 0) {
                m_cvProduce.wait(lock, [this] {
                    return m_bStop || m_queue.size() < m_nMaxQueueSize;
                });
                if (m_bStop) {
                    throw std::runtime_error(
                        "Cannot produce to stopped PyProducerConsumer");
                }
            }
            m_queue.emplace(std::forward<Args>(args)...);
            ++m_nPendingTasks;
            ++m_nProducedCount;
        }
        m_cvConsume.notify_one();
    }

    // 限时生产，超时返回 false
    template <typename... Args>
    bool TryProduce(std::chrono::milliseconds timeout, Args&&... args) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_bStop) {
                return false;
            }
            if (m_nMaxQueueSize > 0) {
                bool bReady = m_cvProduce.wait_for(lock, timeout, [this] {
                    return m_bStop || m_queue.size() < m_nMaxQueueSize;
                });
                if (!bReady || m_bStop) {
                    return false;
                }
            }
            m_queue.emplace(std::forward<Args>(args)...);
            ++m_nPendingTasks;
            ++m_nProducedCount;
        }
        m_cvConsume.notify_one();
        return true;
    }

    // 批量生产（迭代器范围），减少锁竞争
    template <typename Iter>
    void ProduceBatch(Iter begin, Iter end) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_bStop) {
                throw std::runtime_error(
                    "Cannot produce to stopped PyProducerConsumer");
            }
            for (Iter it = begin; it != end; ++it) {
                if (m_nMaxQueueSize > 0) {
                    m_cvProduce.wait(lock, [this] {
                        return m_bStop || m_queue.size() < m_nMaxQueueSize;
                    });
                    if (m_bStop) {
                        throw std::runtime_error(
                            "Cannot produce to stopped PyProducerConsumer");
                    }
                }
                m_queue.emplace(*it);
                ++m_nPendingTasks;
                ++m_nProducedCount;
            }
        }
        m_cvConsume.notify_all();
    }

    void Shutdown() {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_bStop) {
                return;
            }
            m_bStop = true;
        }
        m_cvProduce.notify_all();
        m_cvConsume.notify_all();
        for (auto& worker : m_vecConsumers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        spdlog::info("PyProducerConsumer shutdown complete, produced={}, processed={}",
                     m_nProducedCount.load(), m_nProcessedCount.load());
    }

    void WaitAll() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cvDone.wait(lock, [this] { return m_nPendingTasks.load() == 0; });
    }

    size_t GetQueueSize() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

    size_t GetPendingCount() const {
        return m_nPendingTasks.load();
    }

    size_t GetProcessedCount() const {
        return m_nProcessedCount.load();
    }

    size_t GetProducedCount() const {
        return m_nProducedCount.load();
    }

    size_t GetConsumerCount() const {
        return m_vecConsumers.size();
    }

    size_t GetErrorCount() const {
        return m_nErrorCount.load();
    }

    void ClearErrorCount() {
        m_nErrorCount.store(0);
    }

    bool IsRunning() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return !m_bStop;
    }

    // 禁止拷贝和移动
    PyProducerConsumer(const PyProducerConsumer&) = delete;
    PyProducerConsumer& operator=(const PyProducerConsumer&) = delete;
    PyProducerConsumer(PyProducerConsumer&&) = delete;
    PyProducerConsumer& operator=(PyProducerConsumer&&) = delete;

private:
    void ConsumerLoop(size_t nWorkerId) {
        while (true) {
            std::optional<T> optItem;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cvConsume.wait(lock, [this] {
                    return m_bStop || !m_queue.empty();
                });
                if (m_bStop && m_queue.empty()) {
                    break;
                }
                optItem.emplace(std::move(m_queue.front()));
                m_queue.pop();
                m_cvProduce.notify_one();
            }
            // 先释放 m_mutex 再获取 GIL，避免与主线程死锁：
            // 主线程持有 GIL 调用 Produce（需 m_mutex），
            // 消费者持有 m_mutex 等待 GIL
            {
                GilScopedAcquire gil;
                try {
                    m_funcConsumer(*optItem);
                } catch (const std::exception& e) {
                    spdlog::error("PyProducerConsumer worker {} consumer failed: {}",
                                  nWorkerId, e.what());
                    ++m_nErrorCount;
                }
                {
                    std::lock_guard<std::mutex> lock2(m_mutex);
                    --m_nPendingTasks;
                }
                ++m_nProcessedCount;
                m_cvDone.notify_all();
                // 在 GIL 保护下显式释放 pybind11 对象，
                // 避免 optItem 在循环迭代末尾（无 GIL 时）析构触发 dec_ref 断言
                optItem.reset();
            }
        }
    }

    std::vector<std::thread> m_vecConsumers;
    std::queue<T> m_queue;
    mutable std::mutex m_mutex;
    std::condition_variable m_cvProduce;
    std::condition_variable m_cvConsume;
    std::condition_variable m_cvDone;
    ConsumerFunc m_funcConsumer;
    size_t m_nMaxQueueSize;
    bool m_bStop;
    std::atomic<size_t> m_nPendingTasks;
    std::atomic<size_t> m_nProcessedCount;
    std::atomic<size_t> m_nProducedCount;
    std::atomic<size_t> m_nErrorCount{0};
};
