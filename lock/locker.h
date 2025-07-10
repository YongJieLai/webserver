
#ifndef LOCKER_H
#define LOCKER_H
#include <sys/semaphore.h>
#include <exception>
#include <pthread.h>
class sem{
    public:
        sem() {
            //int sem_init(sem_t *sem, int pshared, unsigned int value);
            // 创建一个未命名的信号量，并设置其初始值。当计数器值 >0 时线程可以获取信号量，=0 时获取会被阻塞
            // pshared=0表示信号量只能在当前进程内使用 pshared=1表示信号量可以在多个进程间共享
            // value表示信号量的初始值
            // 如果sem_init函数调用失败，则抛出异常
            if(sem_init(&m_sem, 0, 0) != 0) {
                throw std::exception();
            }
        }
        sem(int num) {
            // 初始化信号量，num为初始值
            if(sem_init(&m_sem, 0, num) != 0) {
                throw std::exception();
            }
        }
        ~sem() {
            sem_destroy(&m_sem);
        }
        sem(const sem&) = delete;
        sem& operator=(const sem&) = delete;

        // P操作，阻塞等待信号量
        // 如果信号量的值大于0，则将其减1并返回true
        // 如果信号量的值为0，则阻塞等待，直到信号量的值大于0
        // 如果sem_wait函数调用失败，则返回false
        bool wait() { return sem_wait(&m_sem)==0; } 
        
        // V操作，释放信号量
        bool post() {
            // 将信号量的值加1，并唤醒等待该信号量的线程
            // 如果sem_post函数调用失败，则返回false
            return sem_post(&m_sem)==0;
        }
    private:
        sem_t m_sem;
};
class locker{
    public:
        locker(){
            // int pthread_mutex_init(pthread_mutex_t * __restrict, const pthread_mutexattr_t * _Nullable __restrict);
            // 初始化互斥锁，第二个参数为NULL表示使用默认属性
            // 如果pthread_mutex_init函数调用失败，则抛出异常
            if(pthread_mutex_init(&m_mutex, NULL) != 0) {
                throw std::exception();
            }
        }
        ~locker(){
            pthread_mutex_destroy(&m_mutex);
        }
        locker(const locker&) = delete;
        locker& operator=(const locker&) = delete;
        bool lock(){return pthread_mutex_lock(&m_mutex) == 0;} // 锁定互斥锁
        bool unlock(){return pthread_mutex_unlock(&m_mutex) == 0;} // 解锁互斥锁
        pthread_mutex_t *get(){return &m_mutex;} // 获取互斥锁的指针
    private:
        pthread_mutex_t m_mutex;
};
class cond{
    public:
        cond(){
            if(pthread_cond_init(&m_cond, NULL) != 0) {
                throw std::exception();
            }
        }
        ~cond(){
         pthread_cond_destroy(&m_cond);
        }
        cond(const cond&) = delete;
        cond& operator=(const cond&) = delete;
        bool wait(pthread_mutex_t *mutex) {
            // int pthread_cond_wait(pthread_cond_t * __restrict, pthread_mutex_t * __restrict);
            // 等待条件变量，释放互斥锁并阻塞等待条件变量
            // 当条件变量被唤醒时，重新获取互斥锁
            return pthread_cond_wait(&m_cond, mutex) == 0;
        }
        bool signal() {
            // int pthread_cond_signal(pthread_cond_t * __restrict);
            // 唤醒一个等待该条件变量的线程
            return pthread_cond_signal(&m_cond) == 0;
        }
        bool broadcast() {
            // int pthread_cond_broadcast(pthread_cond_t * __restrict);
            // 唤醒所有等待该条件变量的线程
            return pthread_cond_broadcast(&m_cond) == 0;
        }
        bool timewait(pthread_mutex_t *mutex, struct timespec ts) {
            // int pthread_cond_timedwait(pthread_cond_t * __restrict, pthread_mutex_t * __restrict, const struct timespec * __restrict);
            // 等待条件变量，直到超时或被唤醒
            return pthread_cond_timedwait(&m_cond, mutex, &ts) == 0;
        }   
    private:
        pthread_cond_t m_cond; // 条件变量
};
#endif