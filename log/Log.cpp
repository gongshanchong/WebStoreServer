#include "Log.h"
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>

Log::Log(){ logCount = 0; }

Log::~Log(){
    if (logFp != NULL){
        fclose(logFp);
    }

    stop_ = true;
    // 需要注意将已经添加的所有任务执行完
    conditionVariable.notify_all();
    if(logTh.joinable())
        // 阻塞主线程,主线程等待其他子线程执行完毕,一起退出
        logTh.join();
}

// init函数实现日志创建
bool Log::init(const char *file_name, int split_lines){
    // 创建一个写线程，工作线程将要写的内容push进队列，写线程从队列中取出内容，写入日志文件
    // flushLogThread为回调函数,这里表示创建线程异步写日志
    std::function<void()> flush_Log = std::bind(&Log::flushLogThread, this);
    logTh = std::thread(std::move(flush_Log));

    //日志的最大行数
    SplitLines = split_lines;

    // 获取当前时间用于文件命名
    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    //从后往前找到第一个/的位置
    const char *p = strrchr(file_name, '/');
    char log_full_name[256] = {0};

    //相当于自定义日志名
    //若输入的文件名没有/，则直接将时间+文件名作为日志名
    if (p == NULL)
    {
        // snprintf() 会限制输出的字符数，避免缓冲区溢出。size 为要写入的字符的最大数目，超过 size 会被截断，最多写入 size-1 个字符
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }
    else
    {
        //将/的位置向后移动一个位置，然后复制到logname中
        //p - file_name + 1是文件所在路径文件夹的长度
        //dirname相当于./
        strcpy(logName, p + 1);
        strncpy(dirName, file_name, p - file_name + 1);

        //后面的参数跟format有关
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dirName, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, logName);
    }

    toDay = my_tm.tm_mday;

    // a 以附加的方式打开只写文件。若文件不存在，则会建立该文件，如果文件存在，写入的数据会被加到文件尾，即文件原先的内容会被保留。（EOF符保留）
    logFp = fopen(log_full_name, "a");
    if (logFp == NULL)
    {
        return false;
    }

    return true;
}

/*
日志写入前会判断当前day是否为创建日志的时间，行数是否超过最大行限制
若为创建日志时间，写入日志，否则按当前时间创建新log，更新创建时间和行数
若行数超过最大行限制，在当前日志的末尾加count/max_lines为后缀创建新log
*/
//write_log函数完成写入日志文件中的具体内容，主要实现日志分级、分文件、格式化输出内容
//将系统信息格式化后输出，具体为：格式化时间 + 格式化内容
void Log::writeLog(int level, const char *format, ...)
{
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);

    // 该时间用于判断是否当天写的
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0};

    //日志分级
    switch (level)
    {
    case 0:
        strcpy(s, "[debug]:");
        break;
    case 1:
        strcpy(s, "[info]:");
        break;
    case 2:
        strcpy(s, "[warn]:");
        break;
    case 3:
        strcpy(s, "[erro]:");
        break;
    default:
        strcpy(s, "[info]:");
        break;
    }

    // 写入一个log，对m_count++, m_split_lines最大行数
    {
        std::unique_lock<std::mutex> lock(handleMtx);
        //更新现有行数
        logCount++;
        // 日志不是今天的或写入的日志行数是最大行的倍数
        // m_split_lines为最大行数
        if (toDay != my_tm.tm_mday || logCount % SplitLines == 0) //everyday log
        {
            
            char new_log_full_name[256] = {0};
            fflush(logFp);
            fclose(logFp);
            char tail[16] = {0};
        
            // 格式化日志名中的时间部分
            snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);
        
            // 如果是时间不是今天,则创建今天的日志，更新m_today和m_coun
            if (toDay != my_tm.tm_mday)
            {
                snprintf(new_log_full_name, 255, "%s%s%s", dirName, tail, logName);
                toDay = my_tm.tm_mday;
                logCount = 0;
            }
            else
            {
                // 超过了最大行，在之前的日志名基础上加后缀, m_count/m_split_lines
                snprintf(new_log_full_name, 255, "%s%s%s.%lld", dirName, tail, logName, logCount / SplitLines);
            }
            logFp = fopen(new_log_full_name, "a");
        }
    }

    va_list valst;
    // 将传入的format参数赋值给valst，便于格式化输出
    va_start(valst, format);
    std::string logStr;
    {
        std::unique_lock<std::mutex> lock(handleMtx);
        // 写入的具体时间内容格式
        char tail[48] = {0};
        char content[2000] = {0};
        // 时间格式化，snprintf成功返回写字符的总数，其中不包括结尾的null字符
        int n = snprintf(tail, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                        my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                        my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
        logStr += tail;     
        // 内容格式化，用于向字符串中打印数据、数据格式用户自定义，返回写入到字符数组str中的字符个数(不包含终止符)
        int m = vsnprintf(content, 2000, format, valst);
        logStr += content;
        logStr += '\n';

        // 往队列添加元素，需要将所有使用队列的线程先唤醒
        // 当有元素push进队列,相当于生产者生产了一个元素
        // 异步,则将日志信息加入阻塞队列,同步则加锁向文件中写
        logQueue.push(logStr);
    }
    // 通知消费者
    conditionVariable.notify_all();

    va_end(valst);
}

void Log::flush(void)
{
    std::unique_lock<std::mutex> lock(handleMtx);
    // 强制刷新写入流缓冲区
    // 调用fflush()会将缓冲区中的内容写到stream所指的文件中去
    fflush(logFp);
}
