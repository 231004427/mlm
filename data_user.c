#include "data_user.h"
#include "database.h"
int user_updateOnline(int user_id,int on){
	char sql[100]={0};
    int n=snprintf(sql,sizeof(sql),"update user set online=%d where id=%d",on,user_id);
    if(n>sizeof(sql)){
    	return -1;
    }
    if(database_excule(sql)<0){
    	return -2;
    }
	return 1;
}