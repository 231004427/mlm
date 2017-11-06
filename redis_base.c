#include "mlm.h"
#include "redis_base.h"
#include <hiredis-vip/hircluster.h>
redisContext *conRedis;
int redis_base_build(){
    conRedis = redisConnect("127.0.0.1", 6379);
	if (conRedis != NULL && conRedis->err) {
	    err_msg("Error: %s\n", conRedis->errstr);
	    // handle error
	    return -1;
	} else {
	    //err_msg("Connected to Redis\n");
	    return 0;
	}
}

int redis_base_set(char* key,char* value){

	redisReply *reply;
	reply = redisCommand(conRedis, "AUTH sun123");
	//链接丢失
	if (NULL == reply){
		err_msg("redis_base_set  conRedis null");
		return -1;
	}
	freeReplyObject(reply);
	reply = redisCommand(conRedis, "set %s %s",key,value);
	if (reply->type == REDIS_REPLY_STATUS && strcasecmp(reply->str, "OK") == 0){
		freeReplyObject(reply);
		return 0;
	}else{
		err_msg("redis_base_set type error:%s %s",key,value);
    	freeReplyObject(reply);
    	return -1;
	}
}
int redis_base_get_str(char* key,char* value){
    if(value==NULL){
    	err_msg("redis_base_get_str value null");
    	return -1;
    }
	redisReply *reply;
	reply = redisCommand(conRedis, "AUTH sun123");
	//链接丢失
	if (NULL == reply){
		err_msg("redis_base_get_str  conRedis null");
		return -1;
	}
	freeReplyObject(reply);
    reply = redisCommand(conRedis, "get %s",key);
	if (NULL == reply){
		err_msg("redis_base_get_str  conRedis null");
		return -1;
	}
    if(reply && reply->type == REDIS_REPLY_STRING) {
        //printf("get foo => %s\n", reply->str);
        //拷贝数据
        memcpy(value,reply->str,reply->len);
        freeReplyObject(reply);
        return 0;
    }else{
    	err_msg("redis_base_get_str type error:%s %s",key,value);
    	freeReplyObject(reply);
    	return -1;
    }
}
int redis_base_close(){
	redisFree(conRedis);
	return 0;
}