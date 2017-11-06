#ifndef _REDIS_BASE_H_
int redis_base_build();
int redis_base_set(char* key,char* value);
int redis_base_get_str(char* key,char* value);
int redis_base_close();
#define _REDIS_BASE_H_
#endif