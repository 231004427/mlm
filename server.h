#ifndef _SERVER_H_
#define _SERVER_H_
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <arpa/inet.h>

#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>

/* Libevent. */
#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_struct.h>
#include <event2/buffer.h>

#include "workqueue.h"
#include "mylist.h"

/* Port to listen on. */
#define SERVER_PORT 8792
/* Connection backlog (# of backlogged connections to accept). */
#define CONNECTION_BACKLOG 8
/* Socket read and write timeouts, in seconds. */
#define SOCKET_READ_TIMEOUT_SECONDS 10
#define SOCKET_WRITE_TIMEOUT_SECONDS 10
#define NUM_THREADS 1  //处理事件线程池
#define NUM_THREADS2 1 //发送消息线程池
#define TIME_RUN 60//定时任务时间
#define MAX_CLEAN 10//一次性清理最大用户数
#define MAX_NUM_OFF 3 //清理计数，M
//#define TIME_OFF //关闭清理任务

struct timeval timeout_read;
struct timeval timeout_write;
/* threads */
typedef struct {
    pthread_t tid;
    struct event_base *base;
    struct event event;
    int read_fd;
    int write_fd;
}LIBEVENT_THREAD;

typedef struct {
    pthread_t tid;
    struct event_base *base;
}DISPATCHER_THREAD;
LIBEVENT_THREAD *threads;
DISPATCHER_THREAD dispatcher_thread;
int last_thread = 0;

static workqueue_t workqueue;

void send_fd(int sock_fd, int send_fd);
int recv_fd(const int sock_fd);
int setnonblock(int fd);
void closeClientFd(client_t *client);
void closeAndFreeClient(client_t *client);
void server_job_tcp(client_t *this_client);
void server_job_udp(client_t *this_client);
void buffered_on_read(int fd, short what, void* arg);
void buffered_on_write(struct bufferevent *bev, void *arg);
void buffered_on_error(int fd, short what, void *arg);
void thread_libevent_process(int fd, short which, void *arg);
void thread_libevent_tcp(int client_fd,struct event_base *base);
void *worker_thread(void *arg);
void on_accept(int sock, short ev, void *arg);
void on_accept_tcp(int sock, short ev, void *arg);
void on_accept_udp(int sock,struct sockaddr_in * addr);
bool anetKeepAlive(int fd, int interval);
int runServer(void);

#endif
