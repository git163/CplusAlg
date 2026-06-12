#pragma once

#include "interpreter/GilManager.h"
#include <atomic>
#include <condition_variable>
#include <future>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <tuple>
#include <thread>
#include <type_traits>
#include <vector>

// Python 多线程任务线程池
// 专用于 C++ 多线程环境下调度 Python 调用任务
// 工作线程内部自动 GilScopedAcquire / GilScopedRelease
// 使用方式：
//   {
//       GilScopedRelease release;          // 主线程释放 GIL
//       PyThreadPool pool(4);
//       auto optFut = pool.Submit([]() {
//           // 此处已在 GIL 保护下
//           PyModule caller("demo.numpy_ops");
//           return caller.Call("sum_array", arr).cast<double>();
//       });
//       if (optFut) { double result = optFut->get(); }
//   }  // 主线程重新获取 GIL
//
// 参数传递注意：
//   Submit 内部使用 std::make_tuple 打包参数，普通引用会被按值拷贝，
//   std::ref / std::cref 被显式禁止（编译期报错）。
//   如需多线程共享同一份数据，请使用 std::shared_ptr 按值传递：
//
//       auto pData = std::make_shared<std::vector<double>>(data);
//       pool.Submit([pData]() mutable {
//           pData->push_back(3.14);  // 所有消费者操作同一份数据
//       });
//
class PyThreadPool {
public:
    explicit PyThreadPool(size_t nThreads, size_t nMaxQueueSize = 0);
    ~PyThreadPool();

    // 提交任务，如果队列满则阻塞等待，直到有空位或线程池停止
    // 成功返回 std::future，失败（已 stop）返回 std::nullopt
    //
    // 参数安全：
    //   - 所有参数会被拷贝到内部 tuple 中，任务执行时通过 std::apply 调用
    //   - 裸指针、std::ref / std::cref 会在编译期被 static_assert 拒绝
    //   - 如需共享同一份数据，请使用 std::shared_ptr 按值传递
    template <typename F, typename... Args>
    auto Submit(F&& f, Args&&... args)
        -> std::optional<std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>> {
        static_assert(
            (!is_unsafe_task_arg_v<Args> && ...),
            "Task arguments must be passed by value or shared_ptr, raw pointers and references are not allowed");

        using ReturnType
            = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            [func = std::forward<F>(f), tup = std::make_tuple(std::forward<Args>(args)...)]() mutable {
                return std::apply(std::move(func), std::move(tup));
            });

        std::future<ReturnType> result = task->get_future();
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_bStop) {
                return std::nullopt;
            }
            if (m_nMaxQueueSize > 0) {
                m_cvProduce.wait(lock, [this] {
                    return m_bStop || m_tasks.size() < m_nMaxQueueSize;
                });
                if (m_bStop) {
                    return std::nullopt;
                }
            }
            m_tasks.emplace([task]() { (*task)(); });
            ++m_nPendingTasks;
        }
        m_cv.notify_one();
        return result;
    }

    // 等待所有已提交任务执行完毕
    void WaitAll();

    // 优雅关闭：等待所有任务完成后停止工作线程
    void Shutdown();

    size_t GetThreadCount() const {
        return m_vecWorkers.size();
    }

    size_t GetQueueSize() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_tasks.size();
    }

    size_t GetPendingCount() const {
        return m_nPendingTasks.load();
    }

    size_t GetMaxQueueSize() const {
        return m_nMaxQueueSize;
    }

    // 禁止拷贝和移动
    PyThreadPool(const PyThreadPool&) = delete;
    PyThreadPool& operator=(const PyThreadPool&) = delete;
    PyThreadPool(PyThreadPool&&) = delete;
    PyThreadPool& operator=(PyThreadPool&&) = delete;

private:
    template <typename T>
    struct is_reference_wrapper : std::false_type {};

    template <typename U>
    struct is_reference_wrapper<std::reference_wrapper<U>> : std::true_type {};

    template <typename T>
    static constexpr bool is_unsafe_task_arg_v =
        (std::is_pointer_v<std::decay_t<T>> &&
         !std::is_member_function_pointer_v<std::decay_t<T>> &&
         !std::is_function_v<std::remove_pointer_t<std::decay_t<T>>>) ||
        is_reference_wrapper<std::decay_t<T>>::value;

    std::vector<std::thread> m_vecWorkers;
    std::queue<std::function<void()>> m_tasks;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::condition_variable m_cvDone;
    std::condition_variable m_cvProduce;
    std::atomic<size_t> m_nPendingTasks{0};
    size_t m_nMaxQueueSize;
    bool m_bStop{false};
};
