#ifndef _MYLIST_H_
#define _MYLIST_H_
#include <pthread.h>
#include "skiplist.h"
#include "queue.h"
#include <event2/event_struct.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_struct.h>
#include <event2/buffer.h>
#include "cJSON.h"
#include <sys/socket.h>

#define MAX_USER  100 //最大用户数
#define MAX_GROUP  100 //最大房间数
#define MAX_GROUP_USER 5//房间最大用户数
#define MAX_READ_BUF 8000
#define MAX_DATA_BUF 8000
#define TOKEN_LENGTH 32

//用户注册
#define ACTION_USER_REGIST 1
//邀请用户加入房间
#define ACTION_ROOM_INVITE 2
#define ACTION_ROOM_INVITE_YES 3
#define ACTION_ROOM_INVITE_NO 4
//删除房间
#define ACTION_ROOM_DEL 5
//离开房间
#define ACTION_ROOM_LEAVE 6
//创建房间
#define ACTION_ROOM_CREATE 7
//消息单发
#define ACTION_SEND_SINGLE 8
//消息群发
#define ACTION_SEND_ROOM 9
//心跳包
#define ACTION_KEEP_LIVE 10
//系统通知
#define ACTION_SYS_BACK 11
//操作成功
#define ACTION_ACCESS 12
//错误信息
#define ERROR_USER_REGIST_REPEAT 51
#define ERROR_USER_REGIST_MAX 52
#define ERROR_USER_REGIST_SYS 53
#define ERROR_INVITE_NOUSER 54 //用户未注册
#define ERROR_ROOM_INVITE_REPEAT 55
#define ERROR_ROOM_INVITE_INSERT 56
#define ERROR_ROOM_INVITE_NOROOM 57
#define ERROR_ROOM_DEL_NOROOM 58
#define ERROR_ROOM_LEAVE_NOUSER 59
#define ERROR_ROOM_LEAVE_NOROOM 60
#define ERROR_ROOOM_LEAVE_NOREGIST 61
#define ERROR_ROOM_CREATE_NOUSER 62
#define ERROR_ROOM_CREATE_MAX 63
#define ERROR_ROOM_CREATE_REPEAT 64
#define ERROR_ROOM_CREATE_INIT 65 //初始化用户列表错误
#define ERROR_ROOM_CREATE_NEW 66
#define ERROR_ROOM_CREATE_INSERT 67
#define ERROR_SEND_SINGLE_NOUSER 68
#define ERROR_SEND_MULTI_NOROOM 69

#define ERROR_SYS_SEND 70
#define ERROR_SYS_SERVER 71
#define ERROR_SYS_TOKEN 72
#define ERROR_SYS_DATA 73 //数据异常
#define ERROR_SYS_NOREGIST 74
//消息类型
#define MESSAGE_TEXT 101
#define MESSAGE_VOICE 102
#define MESSAGE_IMG 103
#define MESSAGE_FILE 104
#define MESSAGE_VIDEO 105




struct skiplist *client_list;//用户队列
struct skiplist *group_list;//房间队列
//head
struct myhead{
    uint8_t v;
    uint8_t t;
    uint8_t d;
    uint8_t e;
    uint32_t l;
    uint32_t from;
    uint32_t to;
    uint32_t s;
    char token[TOKEN_LENGTH];
};

//client
typedef struct client {
    int fd;
    uint32_t gid;
    uint32_t uid;
    struct event *listenEvent;
    struct myhead myhead;//包头
    char *readbuf;//读缓存
    char *databuf;//数据包
    int size;
    int j;//读取偏移量头部
    int z;//读取偏移量数据
    int keep;//自动断线倒计时
    struct sockaddr_in *address;
    char *token;//token
    //TAILQ_ENTRY(client) entries;
} client_t;
//group
typedef struct
{
    int gid;
    uint32_t uid;
    int type;
    struct skiplist *clients;
} group_t;
//
void list_main(client_t *);//业务处理
int check_token(client_t * client);
int buildData(char * buffer,struct myhead * head,char * data,size_t data_size);//打包数据
int getDataHead(char * src_data,struct myhead * head,size_t head_size);//获取包头信息
char * message_back_build(int type,int n,char *c);//生成消息
int message_send(int fd,char * data_buf,int data_size);//发送数据
void send_back_message(client_t * client,int num,char *msg_char);//系统消息单个
void send_back_group(client_t * client,struct skiplist *client_list,int num,char *msg_char);//系统消息群发

int list_user_regist(client_t *);//加入用户队列
client_t *list_user_search(uint32_t);//查找用户队列
int list_user_remove_uid_pthread(uint32_t);//退出用户队列

int list_group_create(client_t *);//创建房间
int list_group_join(client_t *);//加入房间
int list_group_join_no(client_t *);//拒绝加入
int list_group_remove_user(client_t *);//退出房间
int list_group_remove_gid(client_t *);//删除房间
int list_group_invite(client_t *);//邀请加入

int send_single(client_t *);//单发消息
int send_mulit(client_t *);//群发消息

group_t * list_group_search(int);//查找房间
void list_group_user_off_pthread(client_t *);//用户掉线
int list_user_keeplive(client_t *client);//客户端心跳处理

client_t * buildClient();//初始化用户
void closeAndFreeClient(client_t *client);
void closeClientFd(client_t *client);
void closeClientAll(client_t *client);
#endif

