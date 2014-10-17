#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "errlog.h"

#include "my.h"
#include "mysql/mysql.h"

MYSQL* init_conn_mysql(const char* dbname);
int create_db_mysql();
int mysql_get_int_by_col(char* src_mac, const char* col_name);


extern char* db_user;
extern char* db_pass;

void reg_mysql(char* src_ip, char* src_mac, char* url, char* words, int size)
{

   MYSQL* con=NULL;

   for(;;){
       con=init_conn_mysql("myprivoxy");
       if(con==NULL){
           log_error(LOG_LEVEL_ERROR,\
                     "MySQL failed to connect w/ db. The db should be created right now: %s",\
                     mysql_error(con));

           mysql_close(con);
           if(create_db_mysql()!=0){
               return;
           }
       }else{
           break;
       }
   }

   // UNIX time
   struct timeval tv;
   gettimeofday(&tv,NULL);
  
   char* sql = (char*)calloc(TEXTBUF,sizeof(char));
   if(sql==NULL){
       log_error(LOG_LEVEL_ERROR,"Calloc failed ");
       return;
   }

   sprintf(sql,"SELECT size FROM http_access_data "
               "WHERE src_mac='%s' AND url='%s' AND unixtime>%ld "
               "ORDER BY unixtime DESC",
                src_mac, url, tv.tv_sec-60);

   MYSQL_RES *result;
   MYSQL_ROW row;
   int r=0, isize=0;
   if(mysql_query(con, sql)==0){
       if((result=mysql_store_result(con))){
           r=mysql_num_rows(result);
           if(r>0){
               row=mysql_fetch_row(result);
               if(row[0]!=NULL){
                   isize = atoi(row[0]);

               }else{
                   log_error(LOG_LEVEL_INFO,
                             "size is NULL (sql='%s')",sql);
               }
           }
       }else{
           log_error(LOG_LEVEL_INFO,
	             "mysql_store_result failed %d (sql='%s')",
                      mysql_errno(con),sql);
       }
   }else{
       log_error(LOG_LEVEL_ERROR,
                 "MySQL SELECT query failed %d (sql='%s')",
                  mysql_errno(con), sql);
   }

   if(r>0){
       if(isize<size){

           sprintf(sql,"UPDATE http_access_data SET "
                       "unixtime=%ld, src_ip='%s', words='%s', size=%d "
                       "WHERE url='%s' AND src_mac='%s' AND unixtime>%ld",
                       tv.tv_sec, src_ip, words, size, url, src_mac, tv.tv_sec-60);

           int r;
           if(mysql_query(con, sql)==0){
               if((r=mysql_affected_rows(con))>0){
                   log_error(LOG_LEVEL_INFO,
                             "Updated src_mac=%s, url=%s, size=%d",
                              src_mac,url,size);
               }else{
                   // Not fatal
                   log_error(LOG_LEVEL_INFO,
                             "MySQL UPDATE query failed (sql='%s')",
    			  sql);
               }

           }else{
               log_error(LOG_LEVEL_ERROR,
                     "MySQL UPDATE query failed %d (sql='%s')",
                      mysql_errno(con), sql);
           }
       }else{
           log_error(LOG_LEVEL_INFO,
	   	     "No row shall be updated because of size %d<%d.",
		      isize, size);
       }

   }else{

       sprintf(sql,
               "INSERT INTO http_access_data "
               "(unixtime, src_mac, src_ip, url, words, size) "
               "VALUES ( %ld, '%s', '%s', '%s', '%s', %d )",
               tv.tv_sec, src_mac, src_ip, url, words, size);

       if(mysql_query(con, sql)==0){
           log_error(LOG_LEVEL_INFO,\
                     "Inserted src_mac=%s, src_ip=%s, url=%s",\
                      src_mac,src_ip,url);
       }else{
           log_error(LOG_LEVEL_FATAL,\
                     "MySQL INSERT query failed %d (sql='%s')",\
                      mysql_errno(con), sql);
       }
   }

   mysql_close(con);
   free(sql);
}

MYSQL* init_conn_mysql(const char* dbname)
{

   MYSQL* con=NULL;
   con= mysql_init(NULL);

   if(mysql_real_connect(con,"localhost",db_user,db_pass,dbname,3306,NULL,0)==NULL){
       log_error(LOG_LEVEL_ERROR,\
                 "MySQL failed to connect in creating db: %s",\
                 mysql_error(con));
       return NULL;
   }

   return con;
}

int create_db_mysql(){

   MYSQL* con=init_conn_mysql(NULL); 
   if(con==NULL) return 1;

   char* sql = (char*)calloc(SBUF,sizeof(char));
   if(sql==NULL){
       log_error(LOG_LEVEL_ERROR,"Calloc failed ");
       return 1;
   }
   
   int res=0;
   sprintf(sql,"CREATE DATABASE myprivoxy");
   if(mysql_query(con, sql)!=0){
       res=1;
   }else{
       log_error(LOG_LEVEL_INFO,\
                 "Database successfully created.\n");
   }

   mysql_close(con);
   if(res!=0){
       free(sql);
       return res;
   }

   con=init_conn_mysql("myprivoxy"); 
   if(con!=NULL){
       sprintf(sql,\
               "CREATE TABLE http_access_data "
               "(unixtime LONG, src_mac TEXT, src_ip TEXT, url TEXT, words LONGTEXT, size INT)");

       if(mysql_query(con, sql)!=0){
           log_error(LOG_LEVEL_FATAL,\
                     "MySQL UPDATE query failed (sql='%s')",sql);
           res=1;
       }
   }else{
       res=1;
   }

   mysql_close(con);
   free(sql);
   return res;
}

int mysql_get_int_by_col(char* src_mac, const char* col_name)
{

   MYSQL* con;
   con=init_conn_mysql("myprivoxy");
   if(NULL==con){
       log_error(LOG_LEVEL_FATAL,\
                 "MySQL failed to init %s",\
                 mysql_error(con));
       return 0;
   }

   char* sql = (char*)calloc(128,sizeof(char));
   if(sql==NULL){
       log_error(LOG_LEVEL_ERROR,"Calloc failed ");
       return 0;
   }

   sprintf(sql,"SELECT %s FROM http_access_data WHERE src_mac='%s'",col_name,src_mac);

   MYSQL_RES *result;
   MYSQL_ROW row;
   int r, res=-1;
   if(mysql_query(con, sql)==0){
       if((result=mysql_store_result(con))){
           r=mysql_num_rows(result);
           if(r>0){
               row=mysql_fetch_row(result);
               if(row[0]!=NULL){
                   res = atoi(row[0]);
               }else{
                   log_error(LOG_LEVEL_INFO,"size is NULL (sql='%s')",sql);
               }
           }else{
               res=0;
           }
       }else{
           log_error(LOG_LEVEL_INFO,"mysql_store_result failed %d (sql='%s')",\
                                    mysql_errno(con),sql);
       }
   }else{
       log_error(LOG_LEVEL_ERROR,\
                 "MySQL SELECT query failed %d (sql='%s')",\
                 mysql_errno(con), sql);
   }

   mysql_close(con);
   free(sql);
   return res;
}


