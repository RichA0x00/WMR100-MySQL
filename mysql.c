#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>
#include "mysql.h"

void mysql_print_error(MYSQL *conn)
{
 if(conn != NULL)
 {
   fprintf(stderr,"MySQL Error: %u : %s\n",mysql_errno(conn), mysql_error(conn));
 }
 else
   fprintf(stderr,"MySQL Error - No Handler\n");  
}

MYSQL *mysql_connect(char *hostname, char *username, char *password, char *db_name, unsigned int port_num, char *socket_name, unsigned int flags)
{
    MYSQL *conn;
    conn = mysql_init(NULL); //init Connection handler
    
    if(conn == NULL) {fprintf(stderr,"Error setting up MySQL Handler.\n"); exit(1);}
    if( mysql_real_connect(conn, hostname, username, password, db_name, port_num, socket_name, flags) == NULL )
    {
     mysql_print_error(conn);
     exit(1);
    }
      

    return conn;
}//mysql_connect

