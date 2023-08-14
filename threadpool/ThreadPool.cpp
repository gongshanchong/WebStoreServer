#include "ThreadPool.h"

ThreadPool::ThreadPool(unsigned int size){
    // 启动size个线程
    for(unsigned int i = 0; i < size; ++i){
        // 定义每个线程的工作函数
        // emplace_back向量添加元素的一种方法，直接加入到向量，途中不进行拷贝
        // 线程池只能接受std::function<void()>类型的参数，所以函数参数需要事先使用std::bind(),并且无法得到返回值
        threads.emplace_back(std::thread([this](){
            while(true){
                std::function<void()> task;
                {
                    // 在这个{}作用域内对std::mutex加锁，出了作用域会自动解锁，不需要调用unlock()
                    std::unique_lock<std::mutex> lock(tasksMtx);
                    // 等待条件变量，线程被唤醒后，先重新判断pred的值。如果pred为false，则会释放mutex并重新阻塞然后休眠在wait
                    // 当pred为false时，wait才会阻塞当前线程，为true才会解除阻塞
                    conditionVariable.wait(lock, [this](){
                        return stop_ || !tasks.empty();
                    });
                    // 任务队列为空并且线程池停止，退出线程
                    if(stop_ && tasks.empty()) return;
                    // 从任务队列头取出一个任务
                    task = tasks.front();
                    tasks.pop();
                }
                // 执行任务
                task();
            }
        }));
    }
}

ThreadPool::~ThreadPool(){
    {
        // 在这个{}作用域内对std::mutex加锁，出了作用域会自动解锁，不需要调用unlock()
        std::unique_lock<std::mutex> lock(tasksMtx);
        stop_ = true;
    }

    // 需要注意将已经添加的所有任务执行完
    conditionVariable.notify_all();
    // 让每个线程从内部自动退出
    for(std::thread &th : threads){
        if(th.joinable())
            // 阻塞主线程,主线程等待其他子线程执行完毕,一起退出
            th.join();
    }
}

