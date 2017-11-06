#include "mlm.h"
#include "mylist.h"
#include "data_user.h"
#include "redis_base.h"
#include <event2/event.h>
//业务处理
void list_main(client_t * client)
{
    //判断连接是否有效
    if(client==NULL){
        return;
    }
    if(client->fd<0){
        return;
    }

    client_t *this_client;
    int type=client->myhead.t;
    int result=0;

    if(client->myhead.from==0){
        err_msg("[client:%d] err data",client->fd);
        client->myhead.t=ACTION_SYS_BACK;
        send_back_message(client,ERROR_SYS_DATA,"0");
        #ifndef SERVER_UDP
        closeAndFreeClient(client);
        #endif
        return;
    }
    //判断是否注册
    #ifdef SERVER_UDP
        //注册用户
        this_client=list_user_search(client->myhead.from);
        if(this_client==NULL){
            if(type!=ACTION_USER_REGIST){
                err_msg("[UDP client:%d] err user not regist",client->fd);
                client->myhead.t=ACTION_SYS_BACK;
                send_back_message(client,ERROR_SYS_NOREGIST,"0");
                return;
            }
            this_client=buildClient();
        }
        //复制对象
        this_client->fd=client->fd;
        this_client->myhead=client->myhead;
        this_client->size=client->size;
        memcpy(this_client->address,client->address,sizeof(struct sockaddr_in));
        memcpy(this_client->databuf,client->databuf,client->size);
    #else
        this_client=client;
        if(type!=ACTION_USER_REGIST && type!=ACTION_KEEP_LIVE){
            //TCP
            if(this_client->uid<=0){
                err_msg("[TCP client:%d] err user not regist",this_client->fd);
                this_client->myhead.t=ACTION_SYS_BACK;
                send_back_message(this_client,ERROR_SYS_NOREGIST,"0");
                return;
            }
        }
    #endif
    //判断用户TOKEN
    if(check_token(this_client)<0){
        err_msg("[client:%d] err token",client->fd);
        client->myhead.t=ACTION_SYS_BACK;
        send_back_message(client,ERROR_SYS_TOKEN,"0");
        #ifndef SERVER_UDP
        closeAndFreeClient(client);
        #endif
        return;
    }
    //房间操作加锁
    if(type==ACTION_ROOM_CREATE||type==ACTION_ROOM_LEAVE||type==ACTION_ROOM_INVITE_YES||type==ACTION_ROOM_DEL){
        pthread_rwlock_wrlock(&(group_list->lock));
    }
    //处理动作                 
    if(type==ACTION_USER_REGIST){
        //创建房间
        result = list_user_regist(this_client);
        err_msg("[list_user_regist uid :%d] ",this_client->uid);
        
    }else if(type==ACTION_ROOM_CREATE){
        //创建房间
        result=list_group_create(this_client);
        
    }else if(type==ACTION_ROOM_LEAVE){
        //离开房间
        result=list_group_remove_user(this_client);
        
    }else if(type==ACTION_SEND_SINGLE){
        //单发使用 1文本,2语音，图片3,文件4,视频5
        result=send_single(this_client);
        
    }else if(type==ACTION_SEND_ROOM){
        //房间群发
        result=send_mulit(this_client);

    }else if(type==ACTION_ROOM_INVITE){
        //邀请加入
        result=list_group_invite(this_client);
        
    }else if(type==ACTION_ROOM_INVITE_YES){
        //同意加入
        result=list_group_join(this_client);
        
    }else if(type==ACTION_ROOM_INVITE_NO){
        //拒绝加入
        result=list_group_join_no(this_client);

    }else if(type==ACTION_ROOM_DEL){
        //删除房间
        result=list_group_remove_gid(this_client);
        
    }else if(type==ACTION_KEEP_LIVE){
        //心跳包
        result=list_user_keeplive(this_client);
    }
    //房间操作解锁
    if(type==ACTION_ROOM_CREATE||type==ACTION_ROOM_LEAVE||type==ACTION_ROOM_INVITE_YES||type==ACTION_ROOM_DEL){
        pthread_rwlock_unlock(&(group_list->lock));
    }

    //发送处理结果
    send_back_message(this_client,result,"0");
}
//token
int check_token(client_t *client){
    
    //从Reids获取TOKEN
    if(*client->token==0){
        char key[32];
        sprintf(key, "%d", client->myhead.from);
        redis_base_get_str(key,client->token);
        if(*client->token==0){
            err_msg("[client:%d] no token",client->fd);
            return -1;
        }
    }
    //判断TOKEN是否相同
    err_msg("[client:%d] token1:[%s]token2:[%s]",client->fd,client->token,client->myhead.token);
    if(strcmp(client->token,client->myhead.token)==0){
        return 1;
    }
    return -1;
}
//发送数据
int message_send(int fd,char * data_buf,int data_size)
{
    if(fd>0){
        err_msg("[client:%d] size:%d  message_send",fd,data_size,data_buf);

        if(write(fd,data_buf,data_size)<0){
            err_ret("发送失败");
            return -1;
        }

        return 1;
    }else{
        err_msg("no client error");
        return -1;
    }
}
//系统消息单个
void send_back_message(client_t * client,int num,char *msg_char)
{
    //msg_char=message_back_build(head->t,num,data_buf);
    int msg_l=strlen(msg_char);
    client->myhead.l=msg_l;
    client->myhead.d=num;
    if(buildData(client->databuf,&client->myhead,msg_char,client->myhead.l)<0){
        err_ret("发送失败");
        return;
    }
    int dataSize=sizeof(struct myhead)+msg_l;
    err_msg("[from:%d] type:%d uid:%d d:%d size:%d send_back_message",client->uid,client->myhead.t,client->uid,client->myhead.d,dataSize);
    #ifdef SERVER_UDP
        socklen_t address_size=sizeof(*client->address);
        if(sendto(client->fd,client->databuf,dataSize, 0,client->address,address_size)<0){
            err_ret("发送失败");
        }
    #else
        message_send(client->fd,client->databuf,dataSize);
    #endif
    
}
//系统消息group
void send_back_group(client_t * client,struct skiplist *client_list,int num,char *msg_char)
{
    //msg_char=message_back_build(head->t,num,data_buf);
    int msg_l=strlen(msg_char);
    client->myhead.l=msg_l;
    client->myhead.d=num;
    if(buildData(client->databuf,&client->myhead,msg_char,client->myhead.l)<0){
        err_ret("发送失败");
        return;
    }
    
    int dataSize=sizeof(struct myhead)+msg_l;
    //获取用户
    client_t *client_temp;
    struct skipnode *node;
    struct sk_link *pos = &client_list->head[0];
    struct sk_link *end = &client_list->head[0];
    pos = pos->next;
    skiplist_foreach_forward(pos, end) {
        node = list_entry(pos, struct skipnode, link[0]);
        client_temp=(client_t *)node->value;
        err_msg("[from:%d] type:%d uid:%d d:%d size:%d send_back_group",client->uid,client->myhead.t,client_temp->uid,num,dataSize);
        //发送消息
        #ifdef SERVER_UDP
            socklen_t address_size=sizeof(*client->address);
            if(sendto(client_temp->fd,client->databuf,dataSize, 0,client_temp->address,address_size)<0){
                err_ret("发送失败");
            }
        #else
            message_send(client_temp->fd,client->databuf,dataSize);
        #endif

    }
}
//单发消息
int send_single(client_t * client)
{
    client_t * to_client=list_user_search(client->myhead.to);
    if(to_client!=NULL)
    {
        int dataSize=sizeof(struct myhead)+client->myhead.l;
        err_msg("[to_client:%d] type:%d from:%d d:%d size:%d send_single",to_client->uid,client->myhead.t,client->myhead.from,client->myhead.d,dataSize);
        #ifdef SERVER_UDP
            socklen_t address_size=sizeof(*to_client->address);
            if(sendto(to_client->fd,client->databuf,dataSize, 0,to_client->address,address_size)<0){
                err_ret("发送失败");
            }
        #else
            message_send(to_client->fd,client->databuf,dataSize);
        #endif
        return ACTION_ACCESS;
    }
    else
    {
        //错误无用户
        return ERROR_INVITE_NOUSER;
    }
}
//群发消息
int send_mulit(client_t * client)
{
    pthread_rwlock_rdlock(&(group_list->lock));
    group_t * to_clients=list_group_search(client->myhead.s);
    if(to_clients!=NULL)
    {
        int dataSize=sizeof(struct myhead)+client->myhead.l;
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
            if(client_temp->uid!=client->uid){
                //发送消息
                err_msg("[to_client:%d] type:%d uid:%d d:%d size:%d send_mulit",client_temp->fd,client->myhead.t,client->uid,client->myhead.d,dataSize);
                #ifdef SERVER_UDP
                socklen_t address_size=sizeof(*client_temp->address);
                if(sendto(client_temp->fd,client->databuf,dataSize, 0,client_temp->address,address_size)<0){
                    err_ret("发送失败");
                }
                #else
                    message_send(client_temp->fd,client->databuf,dataSize);
                #endif
            }
        }
        return ACTION_ACCESS;
        
    }else{
        //错误无房间
        return ERROR_SEND_MULTI_NOROOM;
    }
    pthread_rwlock_unlock(&(group_list->lock));
}
int list_user_regist(client_t *client)
{
    //TAILQ_INSERT_TAIL(&groups[0].clients, client, entries);
    int uid=client->myhead.from;
    //系统最大用户数
    if(client_list->count>=MAX_USER)
    {
        //err_msg("err regitst max user");
        //closeAndFreeClient(client);
        return ERROR_USER_REGIST_MAX;
    }
    //已注册
    client_t * client_temp=list_user_search(uid);
    if(client_temp!=NULL)
    {
        err_msg("err regitst repeat user %d %d",client_temp->fd,client->fd);
        if(client_temp->fd!=client->fd){
            closeClientAll(client_temp);
        }else{
            return ACTION_ACCESS;
        }
    }
    //注册会话列表
    struct skipnode *node;
    pthread_rwlock_wrlock(&(client_list->lock));
    node =skiplist_insert(client_list,uid,client);
    pthread_rwlock_unlock(&(client_list->lock));
    if(node==NULL)
    {
        //系统错误
        return ERROR_USER_REGIST_SYS;
    }
    client->uid=uid;
    err_msg("user key :%d\n", client->uid);

    //更新在线状态
    user_updateOnline(uid,1);

    return ACTION_ACCESS;
}
//查找用户队列
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
//创建房间
int list_group_create(client_t *client)
{
    //系统最大房间数
    if(group_list->count>=MAX_GROUP)
    {
        return ERROR_ROOM_CREATE_MAX;
    }
    //err_msg("group_c_uid:%d\n", client->uid);
    //添加房间
    struct skipnode *node;
    
    group_t *group_buf;
    group_buf=list_group_search(client->myhead.s);
    //判断是否已经创建
    if(group_buf==NULL)
    {
        //创建房间
        group_buf=(group_t *)calloc(1,sizeof(group_t));//申请内存
        group_buf->uid=client->uid;
        group_buf->type=0;
        group_buf->gid=client->myhead.s;
        //初始化房间用户列表
        group_buf->clients = skiplist_new();
        if (group_buf->clients==NULL){
            return ERROR_ROOM_CREATE_INIT;
        }
            node =skiplist_insert(group_list, client->myhead.s, group_buf);
            //skiplist_dump(group_list);
            //err_msg("group key:%d value:%p \n", node->key, node->value);
            if(node==NULL){
                return ERROR_ROOM_CREATE_NEW;
            }
    }

    //err_msg("[client:%d] group:%d uid:%d\n", client->fd,client->gid,client->uid);
    return ACTION_ACCESS;
    
}
//邀请加入
int list_group_invite(client_t *client)
{
    client_t * to_client=list_user_search(client->myhead.to);
    if(to_client!=NULL)
    {
        //判断是否已经加入
        //if(client->myhead.s==to_client->gid){
        //    return ERROR_ROOM_INVITE_REPEAT;
        //}
        int dataSize=sizeof(struct myhead)+client->myhead.l;

        #ifdef SERVER_UDP
            socklen_t address_size=sizeof(*to_client->address);
            if(sendto(to_client->fd,client->databuf,dataSize, 0,to_client->address,address_size)<0){
                err_ret("发送失败");
            }
        #else
            message_send(to_client->fd,client->databuf,dataSize);
        #endif

        return ACTION_ACCESS;
    }
    else
    {
        //错误无用户
        return ERROR_INVITE_NOUSER;
    }
}
//加入房间
int list_group_join(client_t *client)
{
    int gid=client->myhead.s;
    
    //err_msg("加入房间qqqq%d",gid);
    group_t *group_temp=list_group_search(gid);
    if(group_temp!=NULL)
    {

        //err_msg("加入房间%d",head->t);
        //退出原来房间
        if(client->gid>0 && client->gid!=gid){
            /*
            struct myhead *head_buf;
            head_buf=(struct myhead *)calloc(1,sizeof(struct myhead));
            memcpy(head_buf,head,sizeof(struct myhead));
            head_buf->t=30;*/
            client->myhead.t=ACTION_ROOM_LEAVE;
            //退出房间
            list_group_remove_user(client);
            /*
            if(head_buf!=NULL){
                free(head_buf);
                head_buf=NULL;
            }*/
            client->myhead.t=ACTION_ROOM_INVITE_YES;
        }
        if(client->gid==0){
            //加入新房间
            struct skipnode *node;
            node=skiplist_insert(group_temp->clients, client->uid, client);
            if(node==NULL){
                //加入房间错误
                return ERROR_ROOM_INVITE_INSERT;
            }
            //加入成功
            client->gid=gid;
        }
        //通知新房间
        send_back_group(client,group_temp->clients,0,"0");
        
    }else{
        //房间不存在
        return ERROR_ROOM_INVITE_NOROOM;
    }
    return ACTION_ACCESS;
}
//拒绝加入
int list_group_join_no(client_t *client)
{
    client_t * to_client=list_user_search(client->myhead.to);
    if(to_client!=NULL)
    {
        int dataSize=sizeof(struct myhead)+client->myhead.l;
        #ifdef SERVER_UDP
            socklen_t address_size=sizeof(*to_client->address);
            if(sendto(to_client->fd,client->databuf,dataSize, 0,to_client->address,address_size)<0){
                err_ret("发送失败");
            }
        #else
            message_send(to_client->fd,client->databuf,dataSize);
        #endif
            
        return ACTION_ACCESS;
    }
    else
    {
        return ERROR_INVITE_NOUSER;
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
//退出房间
int list_group_remove_user(client_t *client)
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
                return ERROR_INVITE_NOUSER;
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
                if(group_temp!=NULL)
                {
                    free(group_temp);
                    group_temp=NULL;
                }
                
            }else{
                //通知房间其他人
                send_back_group(client,group_temp->clients,0,"0");
            }
            //返回消息，退出房间成功
            client->gid=0;
            return ACTION_ACCESS;
        }
        else{
            //返回消息，退出房间失败，没有房间
            return ERROR_ROOM_LEAVE_NOROOM;
        }
    }
    else{
        //返回消息，退出房间失败，用户或房间不存在
        return ERROR_ROOOM_LEAVE_NOREGIST;
    }

}
//删除房间100
int list_group_remove_gid(client_t *client)
{
    int gid=client->myhead.to;
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
                err_msg("[client:%d] type:%d uid:%d gid:%d send_back_group",client_temp->fd,client->myhead.t,client_temp->uid,0);
            }
            //通知用户
            send_back_group(client,group_temp->clients,ACTION_ACCESS,"0");
            //清空用户
            skiplist_delete(group_temp->clients);
            //删除房间
            skiplist_remove(group_list,gid);
            if(group_temp!=NULL)
            {
                free(group_temp);
                group_temp=NULL;
            }
            return ACTION_ACCESS;
        }
    }
    return ERROR_ROOM_DEL_NOROOM;
}
//客户端心跳处理
int list_user_keeplive(client_t *client)
{
    client->keep=0;
    return ACTION_ACCESS;
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
    #ifdef SERVER_UDP
    //读事件
    client->address=calloc(1, sizeof(struct sockaddr_in));
    #else
    client->listenEvent=calloc(1, sizeof(struct event));
    #endif
    //读取缓存
    client->readbuf=(char *)calloc(MAX_READ_BUF,sizeof(char));/*内存在堆上*/
    if(client->readbuf==NULL){
        err_msg("client buffer malloc failed");
        return NULL;
    }
    //数据缓存
    client->databuf=(char *)calloc(MAX_DATA_BUF,sizeof(char));
    if(client->databuf==NULL){
        err_msg("client data_buf malloc failed");
        return NULL;
    }
   //token缓存
    client->token=(char *)calloc(TOKEN_LENGTH,sizeof(char));
    if(client->token==NULL){
        err_msg("client token malloc failed");
        return NULL;
    }
    return client;
    
}
//释放用户退出房间
void list_group_user_off_pthread(client_t *client)
{
    if(client->gid>0 && client->uid >0){
        
        pthread_rwlock_wrlock(&(group_list->lock));
        //err_msg("list_group_user_off %d",client->fd);
        /*
         struct myhead head;
         char *buffer;//缓存
         buffer=(char *)calloc(1024,sizeof(char));//内存在堆上
         if(buffer==NULL){
         err_sys("list_group_user_off buffer");
         }*/
        //退出房间
        client->myhead.t=ACTION_ROOM_LEAVE;
        list_group_remove_user(client);
        /*
         if(buffer!=NULL){
         free(buffer);
         buffer=NULL;
         }*/
        pthread_rwlock_unlock(&(group_list->lock));
    }
}
//释放用户退出主队列
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
    {
        return -1;
    }
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
    /*设置为离线状态*/
    if(client->uid>0){
    user_updateOnline(client->uid,0);
    }
    //释放资源err_msg("释放资源");
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
        if(client->address!=NULL){
            free(client->address);
        }
        if(client->token!=NULL){
            free(client->token);
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
//打包数据
int buildData(char * buffer,struct myhead *_head,char * data,size_t data_size){
    int i;
    int head_size=sizeof(struct myhead);

    struct myhead * head= calloc(1, head_size);
    if(head==NULL){
        return -1;
    }
    memcpy(head,_head,head_size);

    head->l=htonl(head->l);
    head->from=htonl(head->from);
    head->to=htonl(head->to);
    head->s=htonl(head->s);
    
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
    free(head);
    head=NULL;
    return num;
}
//获取包头信息
int getDataHead(char * src_data,struct myhead * head,size_t head_size)
{
    bcopy(src_data,(char *)head,head_size);
    head->l=ntohl(head->l);
    head->from=ntohl(head->from);
    head->to=ntohl(head->to);
    head->s=ntohl(head->s);
    return head_size;
}
//生成JSON消息
char * JSON_build(int type,int code,char *msg,char *text,char *data)
{
    char * msg_char;
    cJSON *root;
    root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "type", type);
    cJSON_AddNumberToObject(root, "errcode", code);
    cJSON_AddStringToObject(root, "errmsg", msg);
    cJSON_AddStringToObject(root, "text",text);
    cJSON_AddStringToObject(root, "data", data);
    msg_char=cJSON_PrintUnformatted(root);
    return msg_char;
}

