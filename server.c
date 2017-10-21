/**
 * MLMerver
 * gcc server.c workqueue.c  -L/usr/local/lib -levent -lpthread
 */
#include "mlm.h"
#include "server.h"
/*Main server.*/
int main(int argc, char *argv[]) {
    return runServer();
}
void handle_pipe(int sig)
{
    err_msg("SIGPIPE");//不做任何处理即可
}
//定时执行
void check_time(int sig)
{
    printf("count %d\n", client_list->count);
    if(client_list->count>0)
    {
        pthread_rwlock_rdlock(&(client_list->lock));
        //获取用户
        client_t *client_temp;
        struct skipnode *node;
        struct sk_link *pos = &(client_list)->head[0];
        struct sk_link *end = &(client_list)->head[0];
        client_t * users[MAX_CLEAN];
        int delNum=0;
        pos = pos->next;
        //读锁
        skiplist_foreach_forward(pos, end) {
            node = list_entry(pos, struct skipnode, link[0]);
            //err_msg("key:0x%08x value:%p \n",node->key, node->value);
            if(node != NULL){
                client_temp=(client_t *)node->value;
                err_msg("fid:%d uid:%d keep:%d",client_temp->fd,client_temp->uid, client_temp->keep);
                client_temp->keep++;
                if(client_temp->keep>MAX_NUM_OFF)
                {
                    users[delNum++]=client_temp;
                    if(delNum>=MAX_CLEAN){
                        break;
                    }
                }
            }
        }
        pthread_rwlock_unlock(&(client_list->lock));
        
        for(int i=0;i<delNum;i++){
            err_msg("check_out %d",users[i]->uid);
            //释放用户
            closeClientAll(users[i]);
        }
        err_msg("check_time end");
    }
}
void timer_task()
{
    signal(SIGALRM, check_time);
    struct itimerval itv;
    itv.it_value.tv_sec = 5;//启动时间
    itv.it_value.tv_usec = 0;
    itv.it_interval.tv_sec = TIME_RUN;//时间间隔
    itv.it_interval.tv_usec = 0;
    if(setitimer(ITIMER_REAL, &itv,NULL)<0){
        err_sys("setitimer failed");
    }
}

/*服务启动初始化*/
int runServer(void)
{
    
	int listenfd;
	struct sockaddr_in listen_addr;
	int reuseaddr_on;

    //异常信号关注
    struct sigaction action;
    action.sa_handler = handle_pipe;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGPIPE, &action, NULL);
    
    //初始化用户队列
    client_list = skiplist_new();
    if (client_list == NULL) {
        err_sys("skiplist failed");
    }
    //初始化锁
    pthread_rwlock_init(&(client_list->lock),NULL);
    
    //初始化会话队列
    group_list = skiplist_new();
    if (group_list==NULL){
        err_sys("group_list failed");
    }
    //初始化锁
    pthread_rwlock_init(&(group_list->lock),NULL);
    

    
    //timeout
    timeout_read.tv_sec=SOCKET_READ_TIMEOUT_SECONDS;
    timeout_write.tv_sec=SOCKET_WRITE_TIMEOUT_SECONDS;

	//Create our listening socket
    #ifdef SERVER_UDP
    listenfd = socket(AF_INET, SOCK_DGRAM, 0);
    #else
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    #endif

	if (listenfd < 0) {
		err_sys("socket failed");
	}
    //端口释放后可立即使用
    //设置超时时间,2分钟
    //anetKeepAlive(listenfd,240);
    //禁止TCP缓存
    #ifndef SERVER_UDP
        reuseaddr_on = 1;
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_on, sizeof(reuseaddr_on));
        setsockopt(listenfd, SOL_SOCKET, SO_KEEPALIVE, (void *)&reuseaddr_on, sizeof(reuseaddr_on));
        setsockopt(listenfd, IPPROTO_TCP, TCP_NODELAY, (void *)&reuseaddr_on, sizeof(reuseaddr_on));
    #endif
    //设置地址
	memset(&listen_addr, 0, sizeof(listen_addr));
	listen_addr.sin_family = AF_INET;
	listen_addr.sin_addr.s_addr = INADDR_ANY;
    //inet_pton(AF_INET,"127.0.0.1",&listen_addr.sin_addr);
	listen_addr.sin_port = htons(SERVER_PORT);
    //绑定服务
	if (bind(listenfd, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0) {
		err_sys("bind failed");
	}
    //TCP
    #ifndef SERVER_UDP
        if (listen(listenfd, CONNECTION_BACKLOG) < 0) {
            err_sys("TCP listen failed");
        }

    #endif

    err_msg("server begin listen ....\n");
	//设置非阻塞模式
	if (setnonblock(listenfd) < 0) {
		err_sys("failed to set server socket to non-blocking");
	}
    //创建线程池//////////////////////////////////////////////////////
    
    if (workqueue_init(&workqueue, NUM_THREADS2)) {
        err_sys("Failed to create work queue");
        close(listenfd);
        workqueue_shutdown(&workqueue);
        return 1;
    }
    //创建定时清理任务
    //timer_task();
    
    //开启多线程事件处理线程池//////////////////////////////////////////////////////
    /*
    int ret;
    int fd[2];
    threads = (LIBEVENT_THREAD *)calloc(NUM_THREADS, sizeof(LIBEVENT_THREAD));
    if (threads == NULL) {
        err_sys("calloc");
        return 1;
    }
    for (i = 0; i < NUM_THREADS; i++) {
        
        ret = socketpair(AF_LOCAL, SOCK_STREAM, 0, fd);
        if (ret == -1) {
            err_sys("threads socketpair()");
            return 1;
        }
        err_msg("read_fd %d write_fd %d",fd[1],fd[0]);
        threads[i].read_fd = fd[1];
        threads[i].write_fd = fd[0];
        threads[i].base = event_base_new();
        if (threads[i].base == NULL) {
            err_sys("threads event_init()");
            return 1;
        }
        
        //注册线程管道可读处理
        event_assign(&threads[i].event,threads[i].base,threads[i].read_fd, EV_READ | EV_PERSIST, thread_libevent_process, &threads[i]);
        
        if (event_add(&threads[i].event, 0) == -1) {
            err_sys("threads event_add()");
            return 1;
        }
    }
    
    for (i = 0; i < NUM_THREADS; i++)
    {
        pthread_create(&threads[i].tid, NULL, worker_thread, &threads[i]);
    }
    */
    
    //主线程连接事件绑定.///////////////////////////////////////////////////////////////////////
    dispatcher_thread.tid = pthread_self();
    dispatcher_thread.base = event_base_new();
    if (dispatcher_thread.base == NULL) {
        err_sys("event_init( base )");
    }
    
    //TCP
    #ifdef SERVER_UDP
        //UDP处理
        on_accept_udp(listenfd,&listen_addr);
    #else
        struct event ListenEvent;
        if(-1==event_assign(&ListenEvent,dispatcher_thread.base,listenfd,EV_READ|EV_PERSIST,on_accept_tcp,NULL))
        {
            err_sys("TCP event_assign error");
        }

        if(-1==event_add(&ListenEvent,NULL))
        {
            err_sys("event_add error");
        }

        err_msg("Server start run ...\n");
        
        /* Start the event loop. */
        if(-1==event_base_dispatch(dispatcher_thread.base))
        {
            err_sys("event_base_dispatch error");
        }
        //释放资源
        struct timeval delay = { 2, 0 };
        event_base_loopexit(dispatcher_thread.base, &delay);
        dispatcher_thread.base = NULL;
    #endif
	close(listenfd);
    pthread_rwlock_destroy(&(client_list->lock));
    pthread_rwlock_destroy(&(group_list->lock));
    
    skiplist_delete(client_list);
    skiplist_delete(group_list);
    
    workqueue_shutdown(&workqueue);
	err_msg("Server shutdown.\n");
    
	return 0;
}
void on_accept_udp(int sock_fd,struct sockaddr_in * addr)
{
    socklen_t peerlen = sizeof(struct sockaddr_in); 
    int databuf_size=MAX_READ_BUF*sizeof(char);
    client_t *client;
    client=buildClient();
    client->fd=sock_fd;
    client->address=addr;

    err_msg("server from %s\n :%d" , inet_ntoa( client->address->sin_addr),ntohs(client->address->sin_port)); 
    int n;  
    while (1)  
    {  
        n = recvfrom(sock_fd, client->databuf, databuf_size, 0,  
                     (struct sockaddr *)client->address, &peerlen);  
        if (n == -1 && errno != EAGAIN) 
        {  
            //if (errno == EINTR)  continue;  
            err_sys("recvfrom failed");  
        }else if (n == 0 || (n==-1 && errno==EAGAIN)){
            continue;
        }  
        else
        {  
            //新建立用户对象
            //client_t *client;
            //client=buildClient();
            //client->address=calloc(1, peerlen);
            //拷贝地址 nc -u 169.254.68.37 8792 
            //memcpy(client->address,&peeraddr,peerlen);
            client->size=n;
            server_job_udp(client);
        }  
    }
}
void on_accept_tcp(int sock, short ev, void *arg)
{
    //err_ret("on_accept_tcp error");
    struct sockaddr_in ClientAddr;
    int nClientSocket = -1;
    socklen_t ClientLen = sizeof(ClientAddr);
    nClientSocket = accept(sock,(struct sockaddr *)&ClientAddr, &ClientLen);
    if(-1==nClientSocket)
    {
        err_ret("accet error");
        return;
    }
    err_msg("[client:%d] connect to server ....",nClientSocket);
    thread_libevent_tcp(nClientSocket,dispatcher_thread.base);
    
    
}
void thread_libevent_tcp(int client_fd,struct event_base *base)
{
    //int ret;
    //char buf[128];
    //通过管道管理用户sock id
    
    if (setnonblock(client_fd) < 0)
        err_ret("failed to set client socket non-blocking");
    
    struct client *client;
    client=buildClient();
    if(client==NULL){
        closeClientAll(client);
        return;
    }
    client->fd = client_fd;
    /*
    if((client->buf_ev = bufferevent_socket_new(base, client_fd, 0))==NULL){
        err_ret("client bufferevent creation failed");
        closeClientFd(client);
        return;
    }
    bufferevent_setcb(client->buf_ev, buffered_on_read, NULL,
                      buffered_on_error, client);
    //设置读写缓存
    //bufferevent_setwatermark(client->buf_ev, EV_READ, 0, MAX_BUFFER);
    //设置超时读写超时
    //bufferevent_set_timeouts(client->buf_ev, &timeout_read, &timeout_write);
    bufferevent_enable(client->buf_ev, EV_READ);
    */
    
    if(-1==event_assign(client->listenEvent,base,client_fd,EV_TIMEOUT|EV_READ|EV_PERSIST,buffered_on_read,client))
    {
        err_sys("event_assign read error");
    }
    if(-1==event_add(client->listenEvent,NULL))
    {
        err_sys("event_add error");
    }
    
    return;
}
/*用户有可读消息，创建消息处理线程*/
void buffered_on_read(int fd, short what, void* arg)
{
    /*
    printf("Got an event on socket %d:%s%s%s%s \n",
           (int) fd,
           (what&EV_TIMEOUT) ? " timeout" : "",
           (what&EV_READ)    ? " read" : "",
           (what&EV_WRITE)   ? " write" : "",
           (what&EV_SIGNAL)  ? " signal" : "");
    struct client *client = (client_t *)arg;*／

    job_t *job;
    if ((job = malloc(sizeof(*job))) == NULL) {
        err_msg("failed to allocate memory for job state");
        closeAndFreeClient(client);
        return;
    }
    job->job_function = server_job_tcp;
    job->user_data = arg;*/
    err_msg("[what:%d]",fd);
    if(what&EV_READ){
        //err_msg("read %d what%d",fd,what);
        //数据读取只能使用单线程，多线程导致同时处理Client带来缓存处理问题
        server_job_tcp(arg);
    }
    else{
        //客户关闭
        err_msg("[client:%d] socket error, disconnecting.",fd);
        closeClientAll(arg);
    }
}
//UDP处理线程
void server_job_udp(client_t *this_client){
    int i;
    int head_size=sizeof(struct myhead);
    socklen_t address_size=sizeof(*this_client->address);
    err_msg("接收到的数据:[%d]",this_client->size);
    //解码头部数据
    if(getDataHead(this_client->databuf,&(this_client->myhead),head_size)<0){
        err_msg("[client:%d] getDataHead faild ",this_client->fd);
    }
    /*
    if(sendto(this_client->fd,this_client->databuf,this_client->size, 0,this_client->address,address_size)<0){
        err_ret("发送失败");
    }*/
    err_msg("from:%d to:%d t:%d d:%d s:%d",this_client->myhead.from,this_client->myhead.to,this_client->myhead.t,this_client->myhead.d,this_client->myhead.s); 
    err_msg("ip:%s %d" , inet_ntoa( this_client->address->sin_addr),ntohs(this_client->address->sin_port)); 
    //业务处理
    list_main(this_client);
}

/*TCP工作线程处理，读消息*/
void server_job_tcp(client_t *this_client)
{
    if(this_client!=NULL)
    {
    	//判断缓存是否为空
    	if(this_client->databuf ==NULL || this_client->readbuf==NULL){
    		//缓存异常
    		err_msg("[client:%d] buffer error.",this_client->fd);
            closeClientAll(this_client);
            return;
    	}
        //建立读取缓存
        int size,i;
        int head_size=sizeof(struct myhead);
        //memset(buffer,0,sizeof(buffer));
        //bzero(&myhead,head_size);
        //int fd=bev->ev_read.ev_fd;
        //printf("读取开始(j(%d),z(%d))\n",this_client->j,this_client->z);
        for (;;) {
            //size = bufferevent_read(bev, buffer, sizeof(buffer));
            size = read(this_client->fd,this_client->readbuf,MAX_READ_BUF);
            //
            if (size == 0) {
                //客户关闭
                err_msg("客户关闭");
                closeClientAll(this_client);
                break;
            }
            if (size==-1) {
                //读取结束
                //err_msg("读取结束\n ");
                break;
            }
            //printf("from server size:%zd\n ",size);
            //return;
            i=0;
            for(i=0;i<size;i++)
            {
                
                if(this_client->j<head_size)
                {
                    this_client->databuf[this_client->j++]=this_client->readbuf[i];
                    //printf("数%d<-%d\n",this_client->j-1,i);
                    if(this_client->j==head_size){//获取完整头部数据，解码
                        if(getDataHead(this_client->databuf,&(this_client->myhead),head_size)<0){
                            err_msg("[client:%d] getDataHead faild ",this_client->fd);
                            break;
                        }
                        /*
                        if(myhead.from>1000){
                            int x;
                            char temp;
                            for(x=0;x<16;x++)
                            {
                                uint8_t temp=data_buf[x];
                                printf("%x",temp);
                            }
                            err_sys("error");
                        }*/
                        //如果数据为空
                        if(this_client->myhead.l==0)
                        {
                            //printf("包结束//////////:长度%d-头部%d-数据%d\n",j,head_size,z);
                            err_msg("[client:%d] type:%d from:%d to:%d size:%d server_job",this_client->fd,this_client->myhead.t,this_client->myhead.from,this_client->myhead.to,this_client->j);
                            //业务处理
                            list_main(this_client);
                            //重置参数,开始接收新包
                            //bzero(&myhead,head_size);
                            //bzero(&data_buf,(1024*sizeof(char)));
                            this_client->z=0;
                            this_client->j=0;
                        }
                        //printf("头部结束:长度%d-数据%d\n",this_client->j,this_client->myhead.l);
                    }
                }else
                {
                    this_client->databuf[this_client->j++]=this_client->readbuf[i];
                    //printf("数%d<-%d\n",this_client->j-1,i);
                    this_client->z++;
                    
                    if(this_client->z==this_client->myhead.l){//收到完整的数据包，否者丢弃
                        //printf("包结束//////////:长度%d-头部%d-数据%d\n",j,head_size,z);
                        err_msg("[client:%d] type:%d from:%d to:%d size:%d server_job",this_client->fd,this_client->myhead.t,this_client->myhead.from,this_client->myhead.to,this_client->j);
                        //业务处理
                        list_main(this_client);
                        //重置参数,开始接收新包
                        //bzero(&myhead,head_size);
                        //bzero(&data_buf,(1024*sizeof(char)));
                        this_client->z=0;
                        this_client->j=0;
                    }
                }
                
            }
        }
    }
}
/*写消息*/
void buffered_on_write(struct bufferevent *bev, void *arg) {
}
/*连接错误消息*/
void buffered_on_error(int fd, short what, void *arg)
{
    
    struct client *client = (client_t *)arg;
            
    if (what & BEV_EVENT_EOF) {
        err_msg("[client:%d] disconnected.",client->fd);
    }
    else {
        err_msg("[client:%d] socket error, disconnecting.",client->fd);
    }
    //释放用户
    closeClientAll(client);
}
/*线程管道事件监控
void thread_libevent_process(int fd, short which, void *arg)
{
    //int ret;
    //char buf[128];
    LIBEVENT_THREAD* me = (LIBEVENT_THREAD*)arg;
    //通过管道管理用户sock id
    int client_fd = recv_fd(me->read_fd);
    
    err_msg("[client:%d] connect to thread[%zd]....",client_fd,me->tid);
    
    if (setnonblock(client_fd) < 0){
        err_ret("failed to set client socket non-blocking");
        return;
    }
    
    struct client *client;
    client = calloc(1, sizeof(*client));
    if (client == NULL)
        err_sys("client malloc failed");
    
    client->fd = client_fd;
    client->gid=0;
    client->uid=0;
    client->keep=0;
    
    if((client->buf_ev = bufferevent_socket_new(me->base, client->fd, 0))==NULL){
        err_ret("client bufferevent creation failed");
        closeAndFreeClient(client);
        return;
    }
    //设置读写缓存
    //bufferevent_setwatermark(client->buf_ev,EV_READ | EV_WRITE, 2, 0);
    bufferevent_setcb(client->buf_ev, buffered_on_read, NULL,
                      buffered_on_error, client);
    //设置超时读写超时
    //bufferevent_set_timeouts(client->buf_ev, &timeout_read, &timeout_write);
    bufferevent_enable(client->buf_ev, EV_READ);
    
    return;
}*/
/*工作线程启动*/
void * worker_thread(void *arg)
{
    
    LIBEVENT_THREAD* me = (LIBEVENT_THREAD*)arg;
    me->tid = pthread_self();
    //event_base_dispatch(me->base);
    
    return NULL;
}
/*处理客户连接
void on_accept(int sock, short ev, void *arg)
{
    
    struct sockaddr_in ClientAddr;
    int nClientSocket = -1;
    socklen_t ClientLen = sizeof(ClientAddr);
    nClientSocket = accept(sock,(struct sockaddr *)&ClientAddr, &ClientLen);
    if(-1==nClientSocket)
    {
        err_ret("accet error");
        return;
    }
    err_msg("[client:%d] connect to server ....",nClientSocket);
    
    //进行数据分发
    int tid = (last_thread + 1) % NUM_THREADS;        //memcached中线程负载均衡算法
    LIBEVENT_THREAD *thread = threads + tid;
    last_thread = tid;
    send_fd(thread->write_fd, nClientSocket);
    
}*/

/* Write "n" bytes to a descriptor. */
ssize_t writen(int fd, const void *vptr, size_t n)
{
    size_t		nleft;
    ssize_t		nwritten;
    const char	*ptr;
    
    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
            if (nwritten < 0 && errno == EINTR)
                nwritten = 0;		/* and call write() again */
            else
                return(-1);			/* error */
        }
        
        nleft -= nwritten;
        ptr   += nwritten;
    }
    return(n);
}
/* end writen */
void Writen(int fd, void *ptr, size_t nbytes)
{
    if (writen(fd, ptr, nbytes) != nbytes)
        err_sys("Writen error");
}

/*管道操作发送*/
void send_fd(int sock_fd, int send_fd)
{
    int ret;
    struct msghdr msg;
    struct cmsghdr *p_cmsg;
    struct iovec vec;
    char cmsgbuf[CMSG_SPACE(sizeof(send_fd))];
    int *p_fds;
    char sendchar = 0;
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);
    p_cmsg = CMSG_FIRSTHDR(&msg);
    p_cmsg->cmsg_level = SOL_SOCKET;
    p_cmsg->cmsg_type = SCM_RIGHTS;
    p_cmsg->cmsg_len = CMSG_LEN(sizeof(send_fd));
    p_fds = (int *)CMSG_DATA(p_cmsg);
    *p_fds = send_fd; // 通过传递辅助数据的方式传递文件描述符
    
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &vec;
    msg.msg_iovlen = 1; //主要目的不是传递数据，故只传1个字符
    msg.msg_flags = 0;
    
    vec.iov_base = &sendchar;
    vec.iov_len = sizeof(sendchar);
    ret = sendmsg(sock_fd, &msg, 0);
    if (ret != 1)
        err_sys("sendmsg");
    err_msg("sock_fd %d send_fd %d",sock_fd,send_fd);
}
/*管道操作获取*/
int recv_fd(const int sock_fd)
{
    int ret;
    struct msghdr msg;
    char recvchar;
    struct iovec vec;
    int recv_fd;
    char cmsgbuf[CMSG_SPACE(sizeof(recv_fd))];
    struct cmsghdr *p_cmsg;
    int *p_fd;
    vec.iov_base = &recvchar;
    vec.iov_len = sizeof(recvchar);
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &vec;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);
    msg.msg_flags = 0;
    
    p_fd = (int *)CMSG_DATA(CMSG_FIRSTHDR(&msg));
    *p_fd = -1;
    
    ret = recvmsg(sock_fd, &msg, 0);
    if (ret != 1)
        err_sys("recvmsg");
    
    p_cmsg = CMSG_FIRSTHDR(&msg);
    if (p_cmsg == NULL)
        err_sys("no passed fd");
    
    
    p_fd = (int *)CMSG_DATA(p_cmsg);
    recv_fd = *p_fd;
    if (recv_fd == -1)
        err_sys("no passed fd");
        err_msg("recv_fd %d sock_fd %d recv_fd %d",ret,sock_fd,recv_fd);
    return recv_fd;
}
/*SOCK设置成不延迟*/
int setnonblock(int fd) {
    int flags;
    
    flags = fcntl(fd, F_GETFL);
    if (flags < 0) return flags;
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0) return -1;
    return 0;
}

/* Set TCP keep alive option to detect dead peers. The interval option
 * is only used for Linux as we are using Linux-specific APIs to set
 * the probe send time, interval, and count. */
bool anetKeepAlive(int fd, int interval)
{
    int val = 1;
    
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val)) == -1)
    {
        err_ret("setsockopt SO_KEEPALIVE");
        return false;
    }
    
#ifdef __LINUX__
    /* Default settings are more or less garbage, with the keepalive time
     * set to 7200 by default on Linux. Modify settings to make the feature
     * actually useful. */
    
    /* 开始首次KeepAlive前的TCP空闲时间,参数的单位为0.5秒,也就是说100秒的空闲时间,参数需要设置为200 */
    val = interval;
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &val, sizeof(val)) < 0) {
        err_ret("setsockopt TCP_KEEPIDLE");
        return false;
    }
    
    /*如果对方没有回ACK包，默认会尝试发送9次，本参数是两次KeepAlive探测间的时间间隔，参数的单位为0.5秒  */
    val = interval/3;
    if (val == 0) val = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &val, sizeof(val)) < 0) {
        err_ret("setsockopt TCP_KEEPINTVL");
        return false;
    }
    /* 判定断开前的KeepAlive探测次数,默认3次*/
    val = 3;
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &val, sizeof(val)) < 0) {
        err_ret("setsockopt TCP_KEEPCNT");
        return false;
    }
#else
    ((void) interval); /* Avoid unused var warning for non Linux systems. */
#endif
    
    return true;
}

