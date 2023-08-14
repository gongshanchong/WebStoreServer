#pragma once
#ifndef LOG_H
#define LOG_H
#include <stdlib.h>
#include <queue>
#include <string>
#include <thread>
#include <mutex>
#include <stdio.h>
#include <cstring>
#include <stdarg.h>
#include <memory>
#include <atomic>
#include <functional>
#include <condition_variable>
#include "../utils/utils.h"

// 单例模式 
class Log
{
private:
    std::thread logTh;
    std::mutex handleMtx;                       // 互斥锁,也成互斥量,可以保护关键代码段,以确保独占式访问
    std::condition_variable conditionVariable;  // 条件变量提供了一种线程间的通知机制,当某个共享数据达到某个值时,唤醒等待这个共享数据的线程
    bool stop_{false};
    FILE *logFp;                                // 打开log的文件指针
    char dirName[128];                          // 路径名
    char logName[128];                          // log文件名
    std::queue<std::string> logQueue;           // 阻塞队列, 异步写入方式，将生产者-消费者模型封装为阻塞队列(当前模式为多个生产者单个消费者)
    int SplitLines;                             // 日志最大行数
    long long logCount;                         // 日志行数记录
    int toDay;                                  // 因为按天分类,记录当前时间是那一天
private:
    Log();
    virtual ~Log();

    //异步写日志方法
    void async_write_log(){
        std::string singleLog;

        // 定义线程的工作函数
        while (true){
            std::unique_lock<std::mutex> lock(handleMtx);
            // 等待条件变量，线程被唤醒后，先重新判断pred的值。如果pred为false，则会释放mutex并重新阻塞然后休眠在wait
            // 当pred为false时，wait才会阻塞当前线程，为true才会解除阻塞
            // 由于写入文件io比较慢，在写文件的时候也可能收到通知，所以从消息队列尽可能多的读取消息(!logQueue.empty())
            conditionVariable.wait(lock, [this](){ return stop_ || !logQueue.empty();});
            // 线程池停止，退出线程
            if(stop_ && logQueue.empty()) return;

            // 由于写入文件io比较慢，在写文件的时候也可能收到通知，所以从消息队列尽可能多的读取消息
            singleLog = logQueue.front();
            logQueue.pop();
            fputs(singleLog.c_str(), logFp);
        }
    }

public:
    //C++11以后,使用局部变量懒汉不用加锁
    static Log *getInstance(){
        static Log instance;
        return &instance;
    }

    //可选择的参数有日志文件、最大行数
    bool init(const char *file_name, int split_lines = 5000000);

    //异步写日志公有方法，调用私有方法async_write_log
    static void flushLogThread(void *args){
        Log::getInstance()->async_write_log();
    }

    //将输出内容按照标准格式整理
    void writeLog(int level, const char *format, ...);

    //强制刷新缓冲区
    void flush(void);
};

//这四个宏定义在其他文件中使用，主要用于不同类型的日志输出
#define LOG_DEBUG(format, ...) Log::getInstance()->writeLog(0, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) Log::getInstance()->writeLog(1, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...) Log::getInstance()->writeLog(2, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) Log::getInstance()->writeLog(3, format, ##__VA_ARGS__)

#endif
