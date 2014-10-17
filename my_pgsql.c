#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "errlog.h"

#include "my.h"
#include <libpq-fe.h>

PGconn* get_pg_conn();
int create_db_pg();
int create_table_pg();
void create_pg_words_hstore(char* words, char* words_hstore);

int global_pg_prepared=0;

extern char* db_user;
extern char* db_pass;
extern int bag_of_words;

void reg_postgresql(char* src_ip, char* src_mac, char* url, char* words, int size)
{

    PGconn *conn;
    PGresult *res;

    conn=get_pg_conn();
	if(conn==NULL){
       log_error(LOG_LEVEL_ERROR,"Failed to get PGconn.");
	   return;
	}

    char* words_hstore = (char*)calloc(TEXTBUF,sizeof(char));
	if(words_hstore==NULL){
		log_error(LOG_LEVEL_ERROR,"Calloc failed ");
		return;
	}

	char* sql = (char*)calloc(TEXTBUF,sizeof(char));
	if(bag_of_words==1){
		create_pg_words_hstore(words,words_hstore);

		sprintf(sql,"INSERT INTO http_access_data (src_ip,src_mac,url,words_hstore,size)"
					"VALUES ('%s','%s','%s','%s','%d')",
					src_ip,src_mac,url,words_hstore,size);
	}else{
		sprintf(sql,"INSERT INTO http_access_data (src_ip,src_mac,url,words,size)"
					"VALUES ('%s','%s','%s','%s','%d')",
					src_ip,src_mac,url,words,size);
	}

    res=PQexec(conn, sql);
	if(PQresultStatus(res) != PGRES_COMMAND_OK)
    	log_error(LOG_LEVEL_ERROR,"INSERT failed -> %s",PQerrorMessage(conn));

    free(sql);
}

PGconn* get_pg_conn()
{

    PGconn *conn=NULL;
    PGresult *res;
    int res_count, flag=0, row;
	char comm[SBUF];

	for(;;){

		sprintf(comm,"host=localhost port=5432 user=%s password=%s dbname=myprivoxy",db_user,db_pass);

	    conn = PQconnectdb(comm);
	    if(PQstatus(conn) == CONNECTION_OK){

			if(global_pg_prepared==1) return conn;

		    res=PQexec(conn, "SELECT tablename from pg_tables");
			if(PQresultStatus(res) != PGRES_TUPLES_OK){
   	    		log_error(LOG_LEVEL_ERROR,"Failed to run select for pg_tables -> %s",
						  PQerrorMessage(conn));
				return NULL;
			}

		    res_count=PQntuples(res);

		    for(row=0;row<res_count;++row){
		        if(strcmp(PQgetvalue(res,row,0),"http_access_data")==0){
   	    			log_error(LOG_LEVEL_INFO,"Found http_access_data table.");
					flag=1;
					break;
		        }
		    }

			PQclear(res);

			if(flag==1){
				global_pg_prepared=1;
				return conn;
			}

		}else{

			sprintf(comm,"host=localhost port=5432 user=%s password=%s",db_user,db_pass);

		    conn = PQconnectdb(comm);
		    if(PQstatus(conn) != CONNECTION_OK){
	       		log_error(LOG_LEVEL_ERROR,"Failed to connect to server -> %s",PQerrorMessage(conn));
				return NULL;
			}

		    res=PQexec(conn, "SELECT datname from pg_database");
			if(PQresultStatus(res) != PGRES_TUPLES_OK){
	       		log_error(LOG_LEVEL_ERROR,"Failed to run select for pg_database -> %s",PQerrorMessage(conn));
				return NULL;
			}

		    res_count=PQntuples(res);

		    for(row=0;row<res_count;++row){
		        if(strcmp(PQgetvalue(res,row,0),"myprivoxy")==0){
	       			log_error(LOG_LEVEL_INFO,"Found myprivoxy database.");
					flag=1;
					break;
		        }
		    }


			if(flag==0){
				if(create_db_pg()==0){
		       		log_error(LOG_LEVEL_ERROR,"Failed to create database.");
					return NULL;
				}
	       		log_error(LOG_LEVEL_INFO,"Created myprivoxy database.");
			}
		}

		if(create_table_pg()==0){
       		log_error(LOG_LEVEL_ERROR,"Failed to create table.");
			return NULL;
		}
		log_error(LOG_LEVEL_INFO,"Created http_access_data table.");


		PQclear(res);
		PQfinish(conn);

	}

	return NULL;
}

int create_db_pg()
{

    PGconn *conn;
    PGresult *res;
	char comm[SBUF];

	sprintf(comm,"host=localhost port=5432 user=%s password=%s",db_user,db_pass);

    conn = PQconnectdb(comm);
    if(PQstatus(conn) != CONNECTION_OK) return 0;

    res=PQexec(conn, "CREATE DATABASE myprivoxy");
    if(PQresultStatus(res) != PGRES_COMMAND_OK ) return 0;

	PQclear(res);
	PQfinish(conn);

	return 1;
}

int create_table_pg()
{

    PGconn *conn;
    PGresult *res;
	char comm[SBUF];

	sprintf(comm,"host=localhost port=5432 user=%s password=%s dbname=myprivoxy",db_user,db_pass);

    conn = PQconnectdb(comm);
    if(PQstatus(conn) != CONNECTION_OK) return 0;

	// Load hstore extension
    res=PQexec(conn, "CREATE EXTENSION hstore");
    if(PQresultStatus(res) != PGRES_COMMAND_OK) return 0;
	
    res=PQexec(conn, "CREATE TABLE http_access_data "
                     "(unixtime INT, src_mac TEXT, src_ip TEXT, url TEXT, words TEXT, words_hstore HSTORE, size INT)");
    if(PQresultStatus(res) != PGRES_COMMAND_OK) return 0;
	
	PQclear(res);
	PQfinish(conn);

	return 1;
}

void create_pg_words_hstore(char* words, char* words_hstore)
{

	char *word,*freq,tmp[SBUF];

	word=strtok(words," ");
	while(word!=NULL){
		freq=strtok(NULL," ");
		if(freq==NULL) break;

		sprintf(tmp,"\"%s\" => %s, ",word,freq);
		strcat(words_hstore,tmp);

		word=strtok(NULL," ");
	}

}


