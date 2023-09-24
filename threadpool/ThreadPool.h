#include <functional>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>

class ThreadPool
{
private:
    std::vector<std::thread> threads;
    std::queue<std::function<void()>> tasks;
    std::mutex tasksMtx;
    std::condition_variable conditionVariable;
    std::atomic<bool> stop_{false};

public:
    ThreadPool(unsigned int size = std::thread::hardware_concurrency());
    ~ThreadPool();

    // void add(std::function<void()>);
    template<class F, class... Args>
    auto add(F&& f, Args&&... args) 
    -> std::future<typename std::__invoke_result<F, Args...>::type>;

};


//不能放在cpp文件，C++编译器不支持模版的分离编译
// 使用add函数前不需要手动绑定参数，而是直接传递，并且可以得到任务的返回值
template<class F, class... Args>
auto ThreadPool::add(F&& f, Args&&... args) -> std::future<typename std::__invoke_result<F, Args...>::type>{
    // 返回值类型
    using return_type = typename std::__invoke_result<F, Args...>::type;

    // 使用智能指针
    // 完美转发参数
    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
    
    // 使用期约
    std::future<return_type> res = task->get_future();
    {
        // 队列锁作用域
        std::unique_lock<std::mutex> lock(tasksMtx);

        // don't allow enqueueing after stopping the pool
        if(stop_)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        // 将任务添加到任务队列
        tasks.emplace([task](){ (*task)(); });
    }
    // 通知一次条件变量, 激活线程
    conditionVariable.notify_one();

    // 返回一个期约
    return res;
}
