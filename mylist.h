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
#define MAX_READ_BUF 8096
#define MAX_DATA_BUF 500000

struct skiplist *client_list;
struct skiplist *group_list;

struct myhead{
    uint8_t v;
    uint8_t t;
    uint16_t d;
    uint32_t l;
    uint32_t from;
    uint32_t to;
};

//client
typedef struct client {
    int fd;
    int32_t gid;
    uint32_t uid;
    struct event *listenEvent;
    struct myhead myhead;//包头
    char *readbuf;//读缓存
    char *databuf;//数据包
    int j;//读取偏移量头部
    int z;//读取偏移量数据
    int keep;
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
void list_main(struct myhead *,char *,struct client *,int);//业务处理
void list_user_regist(client_t *,struct myhead *,char *);//加入用户队列
int list_user_remove_uid_pthread(uint32_t);//退出用户队列
client_t *list_user_search(uint32_t);//查找用户队列
int list_group_create(client_t *,int);//创建房间
group_t * list_group_search(int);//查找房间
void list_group_remove_gid(client_t *,struct myhead *,char *);//删除房间
void list_group_remove_user(client_t *,struct myhead * ,char *);//退出房间
void list_group_user_off_pthread(client_t *);//用户掉线
void list_group_join(client_t *,struct myhead *,char *);//加入房间
client_t * buildClient();//初始化用户
void closeAndFreeClient(client_t *client);
void closeClientFd(client_t *client);
void list_user_keeplive(client_t *client,struct myhead *head,char *data_buf);
void closeClientAll(client_t *client);
#endif

