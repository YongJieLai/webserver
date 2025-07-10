#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include "block_queue.h"

using namespace std;

class Log{
    private:
        Log();
        ~Log();
        void *async_write_log(){}
         char dir_name[128]; //路径名
        char log_name[128]; //log文件名
        int m_split_lines;  //日志最大行数
        int m_log_buf_size; //日志缓冲区大小
        long long m_count;  //日志行数记录
        int m_today;        //因为按天分类,记录当前时间是那一天
        FILE *m_fp;         //打开log的文件指针
        char *m_buf;
        block_queue<string> *m_log_queue; //阻塞队列
        bool m_is_async;                  //是否同步标志位
        locker m_mutex;
        int m_close_log; //关闭日志
        

    public:
        static Log *get_instance(){
            static Log instance;
            return &instance;
        }
        static void *flush_log_thread(void *args){}
        bool init(const char *file_name, int close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);
        void write_log(int level, const char *format, ...); // ...表示可变参数
        // 例如：write_log(1, "This is a log message: %s", "Hello, World!");
        // level可以是日志级别的枚举值，例如：
        // LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR等
        void flush(void); //(void)表示明确无输入
};

#endif // LOG_H