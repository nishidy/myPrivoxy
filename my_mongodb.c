#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "errlog.h"

#include "my.h"
#include <mongoc.h>
#include <bson.h>
#include <bcon.h>
#include <jansson.h>

void reg_mongodb(char* src_ip, char* src_mac, char* url, char* words, int size)
{

/* mongo-c-driver 1.0.0 */

	mongoc_client_t *client;
	mongoc_collection_t *collection;
	mongoc_cursor_t *cursor;

	bson_error_t error;
	bson_oid_t oid;
	const bson_t *doc;
	bson_t *query=NULL, *update=NULL;

	mongoc_init();
	client=mongoc_client_new("mongodb://localhost:27017/");
	collection=mongoc_client_get_collection(client,"myprivoxy","http_access_data");

	query=BCON_NEW("$query",  "{","url",	 BCON_UTF8(url),"}",
				   "$orderby","{","unixtime",BCON_INT32(-1),"}" );

	cursor=mongoc_collection_find(collection,MONGOC_QUERY_NONE,0,0,0,query,NULL,NULL);

	// UNIX time
	struct timeval tv;
	gettimeofday(&tv,NULL);

	// Jansson to parse JSON
	json_t *jroot, *jid, *junixtime, *jsize;
	json_error_t j_error;
	char *str=NULL,*cid=NULL;
	int iunixtime=0,isize=0;

	while(mongoc_cursor_next(cursor, &doc)){

		str=bson_as_json(doc,NULL);
		jroot=json_loads(str,0,&j_error);
		if(!jroot){
			log_error(LOG_LEVEL_ERROR,"%d %s",j_error.line,j_error.text);
			return;
		}

		if(!json_is_object(jroot)){
			log_error(LOG_LEVEL_ERROR,"It is not an object.");
			return;
		}

		jid=json_object_get(jroot,"_id");
		if(!json_is_object(jid)){
			log_error(LOG_LEVEL_INFO,"_id is not an object.");
			return;
		}

		jid=json_object_get(jid,"$oid");
		if(!json_is_string(jid)){
			log_error(LOG_LEVEL_ERROR,"$oid is not an string, %s.", jid->type);
			return;
		}
		cid=(char*)json_string_value(jid);

		junixtime=json_object_get(jroot,"unixtime");
		if(!json_is_integer(junixtime)){
			log_error(LOG_LEVEL_ERROR,"unixtime is not an integer.");
			return;
		}
		iunixtime=json_integer_value(junixtime);

		jsize=json_object_get(jroot,"size");
		if(!json_is_integer(jsize)){
			log_error(LOG_LEVEL_ERROR,"size is not an integer.");
			return;
		}
		isize=json_integer_value(jsize);

		bson_free(str);
		break;
	}

	log_error(LOG_LEVEL_INFO,"%d : %d",iunixtime,tv.tv_sec);

	if(iunixtime<=tv.tv_sec-60){

		if(isize<size){

			bson_oid_init(&oid, NULL);
			doc=BCON_NEW("_id",		BCON_OID(&oid),
						 "unixtime",BCON_INT32(tv.tv_sec),
						 "src_mac",	BCON_UTF8(src_mac),
						 "src_ip",	BCON_UTF8(src_ip),
						 "url",		BCON_UTF8(url),
						 "words",	BCON_UTF8(words),
						 "size",	BCON_INT32(size));

			/*
			doc=bson_new();
			bson_oid_init(&oid, NULL);
			BSON_APPEND_OID(doc,"_id",&oid);
			BSON_APPEND_INT32(doc,"unixtime",tv.tv_sec);
			BSON_APPEND_UTF8(doc,"src_mac",src_mac);
			BSON_APPEND_UTF8(doc,"src_ip",src_ip);
			BSON_APPEND_UTF8(doc,"url",url);
			BSON_APPEND_UTF8(doc,"words",words);
			BSON_APPEND_INT32(doc,"size",size);
			*/

			if(!mongoc_collection_insert(collection,
										 MONGOC_INSERT_NONE,
										 doc,NULL,&error)){
				printf("%s\n",error.message);
			}else{
				char soid[25];
				bson_oid_to_string((const bson_oid_t*)&oid,soid);
				log_error(LOG_LEVEL_INFO,"Successfully inserted $oid %s.",soid);
			}

		}

	}else{

		query=bson_new();
		BSON_APPEND_UTF8(query,"_id",cid);

		update=BCON_NEW("unixtime",BCON_INT32(tv.tv_sec),
						"src_mac", BCON_UTF8(src_mac),
						"src_ip",  BCON_UTF8(src_ip),
						"url",	   BCON_UTF8(url),
						"words",   BCON_UTF8(words),
						"size",	   BCON_INT32(size));

		if(!mongoc_collection_update(collection,
									 MONGOC_UPDATE_NONE,
									 query,update,NULL,&error)){
			printf("%s\n",error.message);
		}else{
			log_error(LOG_LEVEL_INFO,"Successfully updated $oid %s.",cid);
		}
	}

	bson_destroy((bson_t*)doc);
	mongoc_collection_destroy(collection);
	mongoc_client_destroy(client);

}


