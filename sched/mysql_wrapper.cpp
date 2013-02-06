#include "mysql_wrapper.h"
#include <mysql.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
//#include <sstream>

MYSQL *conn;
int get_user_pids(int u_id, std::stringstream& pids);
int mysql_init( const char* u_name, const char* u_pass )
{
    conn = mysql_init(NULL);
    if(conn == NULL)
    {
      // Если дескриптор не получен - выводим сообщение об ошибке
      printf( "Error: can't create MySQL-descriptor\n");
      return -1;
    }
    // Подключаемся к серверу
    if(!mysql_real_connect(conn,
                          NULL,
                          u_name,
                          u_pass,
                           "test",
                          NULL,
                          NULL,
                          0
                          ))
    {
      // Если нет возможности установить соединение с сервером
      // базы данных выводим сообщение об ошибке
      printf(
              "Error: can't connect to database %s\n",
              mysql_error(conn));
      return -1;
    }
    else
    {
      // Если соединение успешно установлено выводим фразу - "Success!"
      printf( "Success!\n");
      return 0;
    }
}

#define MAX_QUERY 100
#define MAX_NAME 50
#define MAX_PASS 50

int auth_user(const char* u_name,const char* u_pass)
{
    MYSQL_RES* res;
    MYSQL_ROW row;
    char query[MAX_QUERY+ MAX_NAME + MAX_PASS];
    //u_name[MAX_NAME -1] = 0;
   // u_pass[MAX_PASS -1] = 0;
    // бибер
    printf("[DB]: trying to auth user %s with pass %s\n",u_name,u_pass);
    sprintf(query,"SELECT u_id FROM users WHERE u_name = '%s' AND u_pass = '%s' ",u_name,u_pass);
    if( mysql_query(conn,query) !=0)
        return -1;

    res = mysql_store_result(conn);
    if(res == NULL)
    {
      printf("Error: can't get the result description\n");
      return -1;
    }

    // Получаем первую строку из результирующей таблицы
    row = mysql_fetch_row(res);
    if(mysql_errno(conn) > 0 || row == NULL)
    {
        printf("Error: can't fetch result\n");
        return -1;
    }
    printf("u_id = %s\n",row[0]);
    return atoi(row[0]);

}

int get_user_pids(const char* u_id, std::stringstream& pids)
{
    char query[MAX_QUERY];
    MYSQL_RES* res;
    MYSQL_ROW row;
    sprintf(query,"SELECT p_id FROM u_projects WHERE u_id = %s",u_id);
    if( mysql_query(conn,query) !=0)
        return -1;

    res = mysql_store_result(conn);
    if(res == NULL)
    {
      printf("Error: can't get the result description\n");
      return -1;
    }

    // Получаем первую строку из результирующей таблицы

    pids<<"Pids=[";
    printf("[DB]: fetching..... %d rows\n",mysql_num_rows(res));

    row = mysql_fetch_row(res);

    if( row != NULL)
        pids<<row[0];

    while( ( row = mysql_fetch_row(res)))
        pids<<','<<row[0];

    pids<<"]\n\n";
    std::cout<<"[DB]: pids: "<<pids.str()<<'\n';

}

#define TBL_PROJECTS_FIELDS 4
int get_project_description(const char* p_id, std::stringstream& desc)
{
    char query[MAX_QUERY];
    MYSQL_RES* res;
    MYSQL_ROW row;
    sprintf(query,"SELECT description, sopath, picpath, name  FROM projects WHERE p_id = %s",p_id);
    if( mysql_query(conn,query) !=0)
        return -1;

    res = mysql_store_result(conn);
    if(res == NULL)
    {
      printf("Error: can't get the result description\n");
      return -1;
    }

    // Получаем первую строку из результирующей таблицы


    printf("[DB]: fetching..... %d rows\n",mysql_num_rows(res));

    row = mysql_fetch_row(res);

    if( row != NULL && mysql_num_rows (res) > 0 )
    {
        int num_fields = mysql_num_fields(res);
        if (num_fields < TBL_PROJECTS_FIELDS)
            return -1;
        desc<<"Description="<<row[0]<<"\nSoPath="<<row[1]<<"\nPicPath="<<row[2]<<"\nName="<<row[3]<<"\n\n";
    }else
    {
        printf("[DB] error fetching:(\n");
        return -1;
    }
    std::cout<<desc.str();
    return 0;

}

/*
int main()
{
    mysql_init("root","123");
    auth_user("root","36666666666666666666666666666666666666666666666666666666666666666666666666666666666666666666666666666666666666666666666666666666666666");
    return 0;
}
*/
