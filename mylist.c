#include "mlm.h"
#include "mylist.h"
//
void list_insert()
{
    
}
//打包数据
int buildData(char * buffer,struct myhead *head,char * data,size_t data_size){
    int i;
    int head_size=sizeof(struct myhead);
    head->d=htons(head->d);
    head->l=htonl(head->l);
    head->from=htonl(head->from);
    head->to=htonl(head->to);
    
    int num=head_size+data_size;
    for(i=0;i<num;i++)
    {
        if(i<head_size){
            buffer[i]=((char *)head)[i];
        }else{
            buffer[i]=data[i-head_size];
        }
        //printf("%d ",buffer[i]);
    }
    return num;
}
//获取包头信息
int getDataHead(char * src_data,struct myhead * head,size_t head_size)
{
    bcopy(src_data,(char *)head,head_size);
    head->d=ntohs(head->d);
    head->l=ntohl(head->l);
    head->from=ntohl(head->from);
    head->to=ntohl(head->to);
    
    return head_size;
}
//生成消息
char * message_back_build(int type,int result)
{
    char * msg_char;
    cJSON *root;
    root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "t", type);
    if(result>0){
        cJSON_AddNumberToObject(root, "n", 1);//成功
        cJSON_AddNumberToObject(root, "c", result);
    }else{
        cJSON_AddNumberToObject(root, "n", result);
        cJSON_AddNumberToObject(root, "c", 0);
    }
    msg_char=cJSON_PrintUnformatted(root);
    return msg_char;
}
//发送数据
void message_send(client_t *client,char * data_buf,int data_size)
{
    if(client!=NULL){
    write(client->fd,data_buf,data_size);
    }else{
       err_msg("message_send:error");
    }
}
//系统消息单个
void send_back_message(client_t * client,struct myhead *head,int result,char *data_buf)
{
    char * msg_char;
    msg_char=message_back_build(head->t,result);
    int msg_l=strlen(msg_char);
    head->t=40;
    head->l=msg_l;
    buildData(data_buf,head,msg_char,head->l);
    err_msg("[client:%d] type:%d uid:%d result:%s send_back_message",client->fd,head->t,client->uid,msg_char);
    message_send(client,data_buf,sizeof(struct myhead)+msg_l);
    
}
//系统消息group
void send_back_group(struct skiplist *client_list,struct myhead *head,int result,char *data_buf)
{
    char * msg_char;
    msg_char=message_back_build(head->t,result);
    int msg_l=strlen(msg_char);
    head->t=40;
    head->l=msg_l;
    buildData(data_buf,head,msg_char,head->l);
    
    //获取用户
    client_t *client_temp;
    struct skipnode *node;
    struct sk_link *pos = &client_list->head[0];
    struct sk_link *end = &client_list->head[0];
    pos = pos->next;
    skiplist_foreach_forward(pos, end) {
        node = list_entry(pos, struct skipnode, link[0]);

        client_temp=(client_t *)node->value;
        err_msg("[client:%d] type:%d uid:%d result:%s send_back_group",client_temp->fd,head->t,client_temp->uid,msg_char);
        //发送消息
        message_send(client_temp,data_buf,sizeof(struct myhead)+msg_l);
    }
}

//业务处理
void list_main(struct myhead *myhead,char * data_buf,struct client *this_client,int data_size)
{
    //用户异常未注册，关闭链接
    if(myhead->t!=10){
        if(this_client->uid<=0){
            err_msg("[client:%d] err user not regist",this_client->fd);
            closeAndFreeClient(this_client);
        }
    }
    
    int type=myhead->t;
    //房间操作加锁
    if(type==20||type==30||type==80||type==100){
        pthread_rwlock_wrlock(&(group_list->lock));
    }

    if (myhead->t==10) {
        //用户注册
        list_user_regist(this_client,myhead,data_buf);
    }else if(myhead->t==20){
        //创建房间
        int result=list_group_create(this_client,myhead->to);
        send_back_message(this_client,myhead,result,data_buf);
        
    }else if(myhead->t==30){//离开房间
        list_group_remove_user(this_client,myhead,data_buf);
    }
    else if(myhead->t>=60 && myhead->t<70){
        //单发使用：60文本,61语音，图片62,文件63,位置64，视频65
        client_t * to_client=list_user_search(myhead->to);
        if(to_client!=NULL)
        {
            message_send(to_client,data_buf,data_size);
        }
        else
        {
            //result=-601,错误无用户
            send_back_message(this_client,myhead,-61,data_buf);
        }
    }else if(myhead->t>=70 && myhead->t<80){
        //房间群发
        pthread_rwlock_rdlock(&(group_list->lock));
        group_t * to_clients=list_group_search(myhead->to);
        
        if(to_clients!=NULL)
        {
            client_t *client_temp;
            //TAILQ_FOREACH(client, &(to_clients->clients), entries){
            // err_msg("to_client %d data:%d\n",client->rid,j);
            //write(client->fd,data_buf,j);
            // }
            //获取用户
            struct skipnode *node;
            struct sk_link *pos = &(to_clients->clients)->head[0];
            struct sk_link *end = &(to_clients->clients)->head[0];
            pos = pos->next;
            skiplist_foreach_forward(pos, end) {
                node = list_entry(pos, struct skipnode, link[0]);
                //err_msg("key:0x%08x value:%p \n",node->key, node->value);
                client_temp=(client_t *)node->value;
                //发送消息
                message_send(client_temp,data_buf,data_size);
            }
            
            
        }else{
            //result=-71,错误无房间
            send_back_message(this_client,myhead,-71,data_buf);
        }
        pthread_rwlock_unlock(&(group_list->lock));
    }else if(myhead->t==50){
        //邀请加入
        //skiplist_dump(client_list);
        client_t * to_client=list_user_search(myhead->to);
        if(to_client!=NULL)
        {
            message_send(to_client,data_buf,data_size);
        }
        else
        {
            //result=-51,错误无用户
            send_back_message(this_client,myhead,-51,data_buf);
        }
    }else if(myhead->t==80){
        //同意加入
        list_group_join(this_client,myhead,data_buf);
    }else if(myhead->t==90){
        //拒绝加入
        client_t * to_client=list_user_search(myhead->to);
        if(to_client!=NULL)
        {
            message_send(to_client,data_buf,data_size);
        }
        else
        {
            //result=-91,错误无用户
            send_back_message(this_client,myhead,-91,data_buf);
        }
    }else if(myhead->t==100){
        //删除房间
        list_group_remove_gid(this_client,myhead,data_buf);
    }else if(myhead->t==200){
        //心跳包
        list_user_keeplive(this_client,myhead,data_buf);
    }
    //房间操作解锁
    if(type==20||type==30||type==80||type==100){
        pthread_rwlock_unlock(&(group_list->lock));
    }
}
//客户端心跳处理
void list_user_keeplive(client_t *client,struct myhead *head,char *data_buf)
{
    client->keep=0;
}
void list_user_regist(client_t *client,struct myhead *head,char *data_buf)
{
    //TAILQ_INSERT_TAIL(&groups[0].clients, client, entries);
    int uid=head->from;
    //已经注册
    if(client->uid>0)
    {
        //err_msg("err regitst again user");
        //closeAndFreeClient(client);
        send_back_message(client,head,-12,data_buf);
        return;
    }
    //系统最大用户数
    if(client_list->count>=MAX_USER)
    {
        //err_msg("err regitst max user");
        //closeAndFreeClient(client);
        send_back_message(client,head,-13,data_buf);
        return;
    }
    //重复注册
    if(list_user_search(uid)!=NULL)
    {
        //err_msg("err regitst repeat user");
        //closeAndFreeClient(client);
        send_back_message(client,head,-14,data_buf);
        return;
    }
  
    //注册会话列表
    struct skipnode *node;
    pthread_rwlock_wrlock(&(client_list->lock));
    node =skiplist_insert(client_list,uid,client);
    pthread_rwlock_unlock(&(client_list->lock));
    if(node==NULL)
    {
        //系统错误
        send_back_message(client,head,-11,data_buf);
        return;
    }
    
    client->uid=uid;
    send_back_message(client,head,uid,data_buf);
    //err_msg("user key :%d\n", client->uid);
}
//查找用户队列10
client_t *list_user_search(uint32_t uid)
{

    if(client_list->count>0){
    pthread_rwlock_rdlock(&(client_list->lock));
    struct skipnode *node=skiplist_search_by_key(client_list,uid);
    pthread_rwlock_unlock(&(client_list->lock));
    //查找为空
    if(node==NULL)
    {
        return NULL;
    }
    client_t *res=(client_t *)node->value;
        return res;
    }
    return NULL;
}
//退出主用户队列10
int list_user_remove_uid_pthread(uint32_t uid)
{
    if(uid>0){
        //锁定
        pthread_rwlock_wrlock(&(client_list->lock));
        err_msg("list_user_remove_uid %d",uid);
        skiplist_remove(client_list,uid);
        //释放
        pthread_rwlock_unlock(&(client_list->lock));
        return 1;
    }
    else
    {return -1;}
}
//创建房间20
int list_group_create(client_t *client,int gid)
{
    //判断用户是否已经注册
    if(client->uid<=0)
    {
        return -22;
    }
    //系统最大房间数
    if(group_list->count>=MAX_GROUP)
    {
        return -23;
    }
    //判断用户已经在其他房间
    if(client->gid>0)
    {
        return -24;
    }
    //err_msg("group_c_uid:%d\n", client->uid);
    //添加房间
    struct skipnode *node;
    
    group_t *group_buf;
    group_buf=list_group_search(gid);
    //判断是否已经创建
    if(group_buf==NULL)
    {
    //创建房间
    group_buf=(group_t *)calloc(1,sizeof(group_t));//申请内存
    group_buf->uid=client->uid;
    group_buf->type=0;
    group_buf->gid=gid;
    //初始化房间用户列表
    group_buf->clients = skiplist_new();
    if (group_buf->clients==NULL){
        return -26;
    }
        node =skiplist_insert(group_list, gid, group_buf);
        //skiplist_dump(group_list);
        //err_msg("group key:%d value:%p \n", node->key, node->value);
        if(node==NULL){
            return -21;
        }
    }
    //默认加入房间用户列表
    node=skiplist_insert(group_buf->clients, client->uid, client);
    if(node==NULL){
        return -27;
    }

    client->gid=gid;
    //err_msg("[client:%d] group:%d uid:%d\n", client->fd,client->gid,client->uid);
    return gid;
    
}
//加入房间80
void list_group_join(client_t *client,struct myhead *head,char *data_buf)
{
    int gid=head->to;
    
    if(client->uid<=0)
    {
        //用户未注册
        send_back_message(client,head,-82,data_buf);
        return;
    }
    if(client->gid==gid)
    {
        //用户已存在
        send_back_message(client,head,-83,data_buf);
        return;
    }
    //err_msg("加入房间qqqq%d",gid);
    group_t *group_temp=list_group_search(gid);
    if(group_temp!=NULL)
    {

        //err_msg("加入房间%d",head->t);
        //退出原来房间
        if(client->gid>0){
            
            struct myhead *head_buf;
            head_buf=(struct myhead *)calloc(1,sizeof(struct myhead));
            memcpy(head_buf,head,sizeof(struct myhead));
            head_buf->t=30;
            
            list_group_remove_user(client,head_buf,data_buf);
            
            if(head_buf!=NULL){
                free(head_buf);
                head_buf=NULL;
            }
        }
        //加入新房间
        struct skipnode *node;
        node=skiplist_insert(group_temp->clients, client->uid, client);
        if(node==NULL){
            //加入房间错误
            send_back_message(client,head,-81,data_buf);
            return;
        }
        
        client->gid=gid;
        //通知新房间
        send_back_group(group_temp->clients,head,gid,data_buf);
        
        
    }else{
        //房间不存在
        send_back_message(client,head,-81,data_buf);
        return;
    }
}
//查找房间
group_t * list_group_search(int gid)
{
    if(group_list->count>0)
    {
        struct skipnode *node=skiplist_search_by_key(group_list,gid);
        //查找为空
        if(node==NULL)
        {
            return NULL;
        }

        group_t *res=(group_t *)node->value;
        return res;
    }
    return NULL;
}
//用户掉线
void list_group_user_off_pthread(client_t *client)
{
    if(client->gid>0 && client->uid >0){

        pthread_rwlock_wrlock(&(group_list->lock));
        //err_msg("list_group_user_off %d",client->fd);
        struct myhead head;
        char *buffer;//缓存
        buffer=(char *)calloc(1024,sizeof(char));/*内存在堆上*/
        if(buffer==NULL){
            err_sys("list_group_user_off buffer");
        }
        head.t=30;
        head.from=0;
        head.to=0;
        //退出房间
        list_group_remove_user(client,&head,buffer);
        
        if(buffer!=NULL){
            free(buffer);
            buffer=NULL;
        }
        
        pthread_rwlock_unlock(&(group_list->lock));
    }
}
//退出房间30
void list_group_remove_user(client_t *client,struct myhead *head,char *data_buf)
{
    
    if(client->gid>0 && client->uid >0){
        //err_msg("list_group_remove_user %d",client->fd);
        //skiplist_dump(group_list);
        group_t *group_temp=list_group_search(client->gid);
        //err_msg(" list_group_search fd:%d",client->fd);
        if(group_temp!=NULL)
        {
            //删除用户
            //err_msg(" 删除用户 fd:%d",client->fd);
            if(group_temp->clients==NULL){
                send_back_message(client,head,-33,data_buf);
                return;
            }
            skiplist_remove(group_temp->clients,client->uid);

            //判断房间用户数是否是0,type=0
            if(group_temp->clients->count==0 && group_temp->type==0)
            {
                
                //释放资源
                //err_msg("释放资源 fd:%d",client->fd);
                skiplist_delete(group_temp->clients);

                //删除房间
                //err_msg("删除房间[%p] fd:%d count:%d",group_list,client->fd,group_list->count);
                skiplist_remove(group_list,client->gid);
                //err_msg("删除房间end fd:%d",client->fd);
                return;
                if(group_temp!=NULL)
                {
                    free(group_temp);
                    group_temp=NULL;
                }
                
            }else{
                //通知房间其他人
                send_back_group(group_temp->clients,head,client->gid,data_buf);
            }
            //返回消息，退出房间成功
            if(head->from>0){
            send_back_message(client,head,client->gid,data_buf);
            client->gid=0;
            }

        }
        else{
            //返回消息，退出房间失败，没有房间
            if(head->from>0){
                send_back_message(client,head,-31,data_buf);
                return;
            }
        }
    }
    else{
        //返回消息，退出房间失败，用户或房间不存在
        if(head->from>0){
            send_back_message(client,head,-32,data_buf);
            return;
        }
    }

}
//删除房间100
void list_group_remove_gid(client_t *client,struct myhead *head,char *data_buf)
{
    int gid=head->to;
    if(gid>0){
        
        group_t *group_temp=list_group_search(gid);
        if(group_temp!=NULL)
        {
            //重置用户房间号=0
            client_t *client_temp;
            struct skipnode *node;
            struct sk_link *pos = &(group_temp->clients)->head[0];
            struct sk_link *end = &(group_temp->clients)->head[0];
            pos = pos->next;
            skiplist_foreach_forward(pos, end) {
                node = list_entry(pos, struct skipnode, link[0]);
                client_temp=(client_t *)node->value;
                client_temp->gid=0;
                err_msg("[client:%d] type:%d uid:%d gid:%d send_back_group",client_temp->fd,head->t,client_temp->uid,0);
            }
            //通知用户
            send_back_group(group_temp->clients,head,gid,data_buf);
            //清空用户
            skiplist_delete(group_temp->clients);
            //删除房间
            skiplist_remove(group_list,gid);
            if(group_temp!=NULL)
            {
                free(group_temp);
                group_temp=NULL;
            }
        }else{
            //房间不存在
            send_back_message(client,head,-101,data_buf);
        }
    }
    else{
        //房间不存在
        send_back_message(client,head,-101,data_buf);
    }
}
//初始化用户
client_t * buildClient(){
    
    //初始化用户
    struct client *client;
    client = calloc(1, sizeof(*client));
    if (client == NULL){
        err_msg("client malloc failed");
        return NULL;
    }
    //读事件监控
    client->listenEvent=calloc(1, sizeof(struct event));
    //读取缓存
    client->readbuf=(char *)calloc(MAX_READ_BUF,sizeof(char));/*内存在堆上*/
    if(client->readbuf==NULL){
        err_msg("client buffer malloc failed");
        return NULL;
    }
    client->databuf=(char *)calloc(MAX_DATA_BUF,sizeof(char));
    if(client->databuf==NULL){
        err_msg("client data_buf malloc failed");
        return NULL;
    }
    
    return client;
    
}
//释放用户
void closeClientAll(client_t *client)
{
    //退出房间
    if(client->gid>0){
        list_group_user_off_pthread(client);
    }
    /* 退出主用户队列*/
    if(client->uid>0){
        list_user_remove_uid_pthread(client->uid);
    }
    //释放资源
    closeAndFreeClient(client);
}
//释放资源
void closeAndFreeClient(client_t *client) {
    if (client != NULL) {
        if (client->listenEvent != NULL) {
            //bufferevent_free(client->buf_ev);
            event_free(client->listenEvent);
            client->listenEvent = NULL;
        }
        closeClientFd(client);
        if(client->readbuf!=NULL){
        free(client->readbuf);
        }
        if(client->databuf!=NULL){
        free(client->databuf);
        }
        free(client);
        client=NULL;
    }
    
}
//关闭客户链接
void closeClientFd(client_t *client) {
    if (client != NULL) {
        if (client->fd >= 0) {
            //直接关闭
            close(client->fd);
            //优雅的关闭
            //shutdown(client->fd,SHUT_RDWR);
            client->fd = -1;
        }
    }
}
