#include "log.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

// 构造函数
Log::Log() : 
    m_count(0),
    m_is_async(false),
    m_fp(nullptr),
    m_buf(nullptr),
    m_log_queue(nullptr),
    m_close_log(0) {
    // 初始化目录和文件名
    memset(dir_name, 0, sizeof(dir_name));
    memset(log_name, 0, sizeof(log_name));
}

// 析构函数（释放资源）
Log::~Log() {
    // 关闭日志文件
    if (m_fp) {
        fclose(m_fp);
        m_fp = nullptr;
    }
    
    // 释放缓冲区
    if (m_buf) {
        delete[] m_buf;
        m_buf = nullptr;
    }
    
    // 释放异步队列
    if (m_log_queue) {
        delete m_log_queue;
        m_log_queue = nullptr;
    }
}

// 异步写日志线程函数
void *Log::flush_log_thread(void *args) {
    return Log::get_instance()->async_write_log();
}

// 后台异步写日志
void *Log::async_write_log() {
    string single_log;
    // 持续从队列中取出日志并写入文件
    while (m_log_queue->pop(single_log)) {
        m_mutex.lock();
        // 写入文件并立即刷新
        fputs(single_log.c_str(), m_fp);
        fflush(m_fp);
        m_mutex.unlock();
    }
    return nullptr;
}

// 初始化日志系统
bool Log::init(const char *file_name, int close_log, int log_buf_size, 
               int split_lines, int max_queue_size) {
    // 设置是否关闭日志
    m_close_log = close_log;
    m_log_buf_size = log_buf_size;
    m_split_lines = split_lines;

    // 异步模式设置
    if (max_queue_size >= 1) {
        m_is_async = true;
        // 创建阻塞队列
        m_log_queue = new block_queue<string>(max_queue_size);
        
        // 创建后台写线程
        pthread_t tid;
        if (pthread_create(&tid, NULL, flush_log_thread, NULL) != 0) {
            delete m_log_queue;
            m_log_queue = nullptr;
            return false;
        }
        // 分离线程（自动回收资源）
        pthread_detach(tid);
    }
    
    // 分配日志缓冲区
    m_buf = new char[m_log_buf_size];
    memset(m_buf, '\0', m_log_buf_size);
    
    // 获取当前时间
    time_t t = time(NULL);
    struct tm sys_tm;
    localtime_r(&t, &sys_tm);

    // 解析文件路径
    const char *p = strrchr(file_name, '/');
    char log_full_name[256] = {0};

    if (p == NULL) {
        // 只有文件名，没有路径
        snprintf(log_name, 128, "%s", file_name);
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", 
                sys_tm.tm_year + 1900, sys_tm.tm_mon + 1, sys_tm.tm_mday, log_name);
    } else {
        // 分离目录和文件名
        strncpy(log_name, p + 1, 127);
        strncpy(dir_name, file_name, p - file_name + 1);
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", 
                dir_name, sys_tm.tm_year + 1900, sys_tm.tm_mon + 1, sys_tm.tm_mday, log_name);
    }
    
    m_today = sys_tm.tm_mday;
    
    // 创建日志目录（如果不存在）
    if (dir_name[0] != '\0') {
        DIR* dir = opendir(dir_name);
        if (dir == NULL) {
            mkdir(dir_name, 0755);
        } else {
            closedir(dir);
        }
    }

    // 打开日志文件
    m_fp = fopen(log_full_name, "a");
    if (m_fp == NULL) {
        // 文件打开失败，尝试创建目录后重试
        if (dir_name[0] != '\0') {
            mkdir(dir_name, 0755);
            m_fp = fopen(log_full_name, "a");
        }
        if (m_fp == NULL) {
            return false;
        }
    }

    return true;
}

// 写入日志（核心方法）
void Log::write_log(int level, const char *format, ...) {
    // 获取当前时间（微秒精度）
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm sys_tm;
    localtime_r(&t, &sys_tm);
    
    // 日志级别标签
    const char *level_str = "";
    switch (level) {
        case 0: level_str = "[debug]:"; break;
        case 1: level_str = "[info]:"; break;
        case 2: level_str = "[warn]:"; break;
        case 3: level_str = "[error]:"; break;
        default: level_str = "[info]:"; break;
    }
    
    // 加锁（整个日志写入过程原子化）
    m_mutex.lock();
    
    // 更新日志行计数
    m_count++;
    
    // 检查是否需要日志文件分割（按天或按大小）
    if (m_today != sys_tm.tm_mday || m_count % m_split_lines == 0) {
        char new_log[256] = {0};
        fflush(m_fp);
        fclose(m_fp);
        
        // 日期部分字符串
        char date_str[16] = {0};
        snprintf(date_str, 16, "%d_%02d_%02d_", 
                sys_tm.tm_year + 1900, sys_tm.tm_mon + 1, sys_tm.tm_mday);
        
        if (m_today != sys_tm.tm_mday) {
            // 按天分割：2023_08_01_logname
            snprintf(new_log, 255, "%s%s%s", dir_name, date_str, log_name);
            m_today = sys_tm.tm_mday;
            m_count = 0;
        } else {
            // 按大小分割：2023_08_01_logname.1
            snprintf(new_log, 255, "%s%s%s.%lld", 
                    dir_name, date_str, log_name, m_count / m_split_lines);
        }
        
        // 打开新日志文件
        m_fp = fopen(new_log, "a");
        if (m_fp == NULL) {
            // 失败时尝试恢复原文件
            char fallback_log[256] = {0};
            snprintf(fallback_log, 255, "%s%s", dir_name, log_name);
            m_fp = fopen(fallback_log, "a");
        }
    }
    
    // 构建日志前缀 [年-月-日 时:分:秒.微秒 日志级别]
    int prefix_len = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                            sys_tm.tm_year + 1900, sys_tm.tm_mon + 1, sys_tm.tm_mday,
                            sys_tm.tm_hour, sys_tm.tm_min, sys_tm.tm_sec, now.tv_usec, level_str);
    
    // 处理可变参数
    va_list valst;
    va_start(valst, format);
    
    // 格式化用户日志内容
    int content_len = vsnprintf(m_buf + prefix_len, m_log_buf_size - prefix_len - 2, format, valst);
    va_end(valst);
    
    // 添加换行符和结束符
    m_buf[prefix_len + content_len] = '\n';
    m_buf[prefix_len + content_len + 1] = '\0';
    
    string log_str(m_buf);
    
    // 根据模式写入日志
    if (m_is_async && m_log_queue && !m_log_queue->full()) {
        // 异步模式：日志入队
        m_log_queue->push(log_str);
    } else {
        // 同步模式：直接写文件
        fputs(log_str.c_str(), m_fp);
    }
    
    // 操作完成，释放锁
    m_mutex.unlock();
}

// 刷新日志缓冲区
void Log::flush(void) {
    m_mutex.lock();
    if (m_fp) {
        // 强制刷新文件缓冲区到磁盘
        fflush(m_fp);
    }
    m_mutex.unlock();
}