#pragma once

#include "interpreter/GilManager.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <spdlog/spdlog.h>
#include <thread>
#include <vector>

// Python 多线程批量生产者-消费者模型
// 消费者一次批量取出多条数据后统一处理，减少 GIL 获取/释放开销和锁竞争
// 适合 Python 侧需要批量处理的场景（如 NumPy batch 计算）
// 使用方式：
//   PyBatchConsumer<TaskData> bc(2, [](std::vector<TaskData>& batch) {
//       // 此处已在 GIL 保护下
//       PyModule caller("demo.numpy_ops");
//       caller.Call("process_batch", batch);
//   }, 10, std::chrono::milliseconds(100), 100);
//   bc.Produce(std::move(data));
//   bc.Flush();
//   bc.Shutdown();
template <typename T>
class PyBatchConsumer {
public:
    using BatchConsumerFunc = std::function<void(std::vector<T>&)>;

    explicit PyBatchConsumer(size_t nConsumers,
                             BatchConsumerFunc func,
                             size_t nBatchSize,
                             std::chrono::milliseconds timeout = std::chrono::milliseconds(100),
                             size_t nMaxQueueSize = 0)
        : m_funcConsumer(std::move(func)),
          m_nBatchSize(nBatchSize),
          m_timeout(timeout),
          m_nMaxQueueSize(nMaxQueueSize),
          m_bStop(false),
          m_bFlushRequested(false),
          m_nPendingTasks(0),
          m_nProcessedCount(0),
          m_nProducedCount(0) {
        assert(nConsumers > 0 && "nConsumers must be positive");
        assert(nBatchSize > 0 && "nBatchSize must be positive");
        for (size_t i = 0; i < nConsumers; ++i) {
            m_vecConsumers.emplace_back([this, i]() {
                ConsumerLoop(i);
            });
        }
        spdlog::info("PyBatchConsumer created with {} consumers, batchSize={}, timeoutMs={}, maxQueueSize={}",
                     nConsumers, nBatchSize, timeout.count(), nMaxQueueSize);
    }

    ~PyBatchConsumer() {
        Shutdown();
    }

    // 完美转发生产
    template <typename... Args>
    void Produce(Args&&... args) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_bStop) {
                throw std::runtime_error(
                    "Cannot produce to stopped PyBatchConsumer");
            }
            if (m_nMaxQueueSize > 0) {
                m_cvProduce.wait(lock, [this] {
                    return m_bStop || m_queue.size() < m_nMaxQueueSize;
                });
                if (m_bStop) {
                    throw std::runtime_error(
                        "Cannot produce to stopped PyBatchConsumer");
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

    // 批量生产（迭代器范围）
    template <typename Iter>
    void ProduceBatch(Iter begin, Iter end) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_bStop) {
                throw std::runtime_error(
                    "Cannot produce to stopped PyBatchConsumer");
            }
            for (Iter it = begin; it != end; ++it) {
                if (m_nMaxQueueSize > 0) {
                    m_cvProduce.wait(lock, [this] {
                        return m_bStop || m_queue.size() < m_nMaxQueueSize;
                    });
                    if (m_bStop) {
                        throw std::runtime_error(
                            "Cannot produce to stopped PyBatchConsumer");
                    }
                }
                m_queue.emplace(*it);
                ++m_nPendingTasks;
                ++m_nProducedCount;
            }
        }
        m_cvConsume.notify_all();
    }

    // 强制刷出当前所有待消费数据（即使不足 batch size）
    void Flush() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_bStop) {
                return;
            }
            m_bFlushRequested = true;
        }
        m_cvConsume.notify_all();
        // 等待队列清空或 flush 完成
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cvDone.wait(lock, [this] {
            return m_queue.empty() && m_nPendingTasks.load() == 0;
        });
        m_bFlushRequested = false;
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
        spdlog::info("PyBatchConsumer shutdown complete, produced={}, processed={}",
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
    PyBatchConsumer(const PyBatchConsumer&) = delete;
    PyBatchConsumer& operator=(const PyBatchConsumer&) = delete;
    PyBatchConsumer(PyBatchConsumer&&) = delete;
    PyBatchConsumer& operator=(PyBatchConsumer&&) = delete;

private:
    void ConsumerLoop(size_t nWorkerId) {
        while (true) {
            std::vector<T> batch;
            bool bShouldProcess = false;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cvConsume.wait(lock, [this] {
                    return m_bStop || m_bFlushRequested || !m_queue.empty();
                });

                if (m_bStop && m_queue.empty()) {
                    break;
                }

                // 尝试凑够 batch size
                while (!m_queue.empty() && batch.size() < m_nBatchSize) {
                    batch.emplace_back(std::move(m_queue.front()));
                    m_queue.pop();
                }

                bShouldProcess = !batch.empty();
                // 如果凑够了 batch size，或正在 flush/shutdown，则立即处理
                if (batch.size() < m_nBatchSize && !m_bStop && !m_bFlushRequested) {
                    // 不足 batch size，等待更多数据或超时
                    auto status = m_cvConsume.wait_for(lock, m_timeout);
                    if (status == std::cv_status::timeout && !batch.empty()) {
                        bShouldProcess = true;
                    } else if (!m_queue.empty() && batch.size() < m_nBatchSize) {
                        // 被唤醒且队列有新数据，继续凑
                        while (!m_queue.empty() && batch.size() < m_nBatchSize) {
                            batch.emplace_back(std::move(m_queue.front()));
                            m_queue.pop();
                        }
                        bShouldProcess = true;
                    } else if (m_bStop || m_bFlushRequested) {
                        bShouldProcess = !batch.empty();
                    }
                }
            }

            if (bShouldProcess) {
                m_cvProduce.notify_all();
                try {
                    GilScopedAcquire gil;
                    m_funcConsumer(batch);
                } catch (const std::exception& e) {
                    spdlog::error("PyBatchConsumer worker {} batch consumer failed: {}",
                                  nWorkerId, e.what());
                    ++m_nErrorCount;
                }
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_nPendingTasks -= batch.size();
                }
                m_nProcessedCount += batch.size();
                m_cvDone.notify_all();
            } else if (m_bStop) {
                break;
            }
        }
    }

    std::vector<std::thread> m_vecConsumers;
    std::queue<T> m_queue;
    mutable std::mutex m_mutex;
    std::condition_variable m_cvProduce;
    std::condition_variable m_cvConsume;
    std::condition_variable m_cvDone;
    BatchConsumerFunc m_funcConsumer;
    size_t m_nBatchSize;
    std::chrono::milliseconds m_timeout;
    size_t m_nMaxQueueSize;
    std::atomic<bool> m_bStop{false};
    std::atomic<bool> m_bFlushRequested{false};
    std::atomic<size_t> m_nPendingTasks;
    std::atomic<size_t> m_nProcessedCount;
    std::atomic<size_t> m_nProducedCount;
    std::atomic<size_t> m_nErrorCount{0};
};
