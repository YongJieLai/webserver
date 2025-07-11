#include "lst_timer.h"
#include "../http/http_conn.h"

sort_timer_lst::sort_timer_lst(){
    head=NULL;
    tail=NULL;
}
sort_timer_lst::~sort_timer_lst(){
    util_timer *tmp=head;
    while(tmp){
        head=tmp->next;
        delete tmp;
        tmp=head;
    }
}

void sort_timer_lst::add_timer(util_timer *timer){
    if(!timer)return;
    if(!head){
        head=tail=timer;
        return;
    }
    add_timer(timer,head);
}
void sort_timer_lst::add_timer(util_timer *timer,util_timer *lst_head){
    util_timer *prev=lst_head;
    util_timer *tmp=prev->next;
    while(tmp){
        if(timer->expire<tmp->expire){
            prev->next=timer;
            timer->next=tmp;
            tmp->prev=timer;
            timer->prev=prev;
            break;
        }
        prev=tmp;
        tmp=tmp->next;
    }
    if(!tmp){
        prev->next=timer;
        timer->prev=prev;
        timer->next=NULL;
        tail=timer;
    }
}

void sort_timer_lst::adjust_timer(util_timer *timer){
    if(!timer)return;
    util_timer *tmp=timer->next;
    if (!tmp || (timer->expire < tmp->expire))
    {
        return;
    }
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    }
    else
    {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}
void sort_timer_lst::del_timer(util_timer *timer){
     if (!timer)
    {
        return;
    }
    if ((timer == head) && (timer == tail))
    {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    if (timer == tail)
    {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}
void sort_timer_lst::tick(){
     if (!head)
    {
        return;
    }
    
    time_t cur = time(NULL);
    util_timer *tmp = head;
    while (tmp)
    {
        if (cur < tmp->expire) break;
        tmp->cb_func(tmp->user_data);
        head = tmp->next;
        if (head) head->prev = NULL;
        delete tmp;
        tmp = head;
    }
}

void Utils::init(int timeslot){
    m_TIMESLOT=timeslot;    
}

int Utils::setnonblocking(int fd){
    int old_option =fcntl(fd,F_GETFL);
    int new_option = old_option|O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

//将内核事件表注册读事件 ET 模式 开启边缘触发
//在 addfd 函数中先注册 epoll 事件再设置非阻塞模式的设计
/*
    **确保边缘触发（ET）模式的数据完整性**
    ET 模式只在文件描述符状态变化时通知一次。如果先设置非阻塞模式
    可能在注册 epoll 前就有数据到达（比如连接建立时的快速重传)
    此时内核不会通知应用层，导致数据滞留缓冲区却无法触发事件。
    后置设置非阻塞从根本上避免了这种'静默数据丢失'的风险。

    **维持状态变更的原子性**
    Linux 内核处理 epoll 注册时存在微秒级的时间窗口。
    假设时序如下：设置非阻塞 -> 数据到达 -> 注册 epoll，这个间隙就会导致数据就绪事件被遗漏。
    而现有顺序将'可读事件监控'和'I/O行为模式'两个变更解耦，确保监控生效前不会产生不可控的 I/O 行为。

    **工程实践的最佳平衡**
    虽然技术上可以通过添加额外补救措施（如注册后主动 recv 探测）来支持调换顺序，但这会增加系统调用次数和复杂度。
    当前方案在安全性和性能上达到最优平衡——只需两次确定性的系统调用（epoll_ctl + fcntl），且完全规避时序风险。
*/
void Utils::addfd(){
    epoll_event event;
    event.data.fd=fd;
    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void Utils::sig_handler(int sig){
    int save_errno=errno;
    int msg=sig;
    send(u_pipefd[1],(char *)&msg,1,0);
    errno=save_errno;
}

void Utils::addsig(int sig, void(handler)(int), bool restart) {
    struct sigaction sa;            // 创建信号动作结构
    memset(&sa, '\0', sizeof(sa));  // 清零初始化（关键安全步骤）
    sa.sa_handler = handler;        // 绑定用户自定义处理函数
    if (restart)                    // 系统调用重启标志
        sa.sa_flags |= SA_RESTART;  // 启用被中断系统调用自动恢复
    sigfillset(&sa.sa_mask);        // 阻塞所有其他信号（安全隔离）
    assert(sigaction(sig, &sa, NULL) != -1); // 绑定信号与处理行为
}

void Utils::timer_handler(){
    m_timer_lst.tick();
    alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char *info){
    send(connfd,info,strlen(info),0);
    close(connfd);
}
int *Utils::u_pipefd=0;
int Utils::u_epollfd=0;

class Utils;
void cb_func(client_data *user_data){
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
}
