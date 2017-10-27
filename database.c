#include "database.h"
#include "mysql.h"
MYSQL *conn_ptr;
MYSQL_RES *res;
MYSQL_ROW row;
int database_build(){
    char *host = "127.0.0.1";
    char *user = "root";
    char *password = "123";
    char *db = "playcat";
    unsigned int port = 3306;
    char *unix_socket = NULL;
    unsigned long client_flag = 0;
	conn_ptr = mysql_init(NULL);
    if(!conn_ptr){return(-1);}
    conn_ptr = mysql_real_connect(conn_ptr, host, user, password, db, port, unix_socket, client_flag);
	if(!conn_ptr){return(-1);}
	return 1;
}
int database_excule(char* sql){
	mysql_query(conn_ptr,sql);
    if( mysql_query(conn_ptr,sql))
    {
        return -1;
    }
    return 0;
}
int database_close(){
	mysql_close(conn_ptr);
	return 0;
}