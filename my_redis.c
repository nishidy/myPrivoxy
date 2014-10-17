#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <malloc.h>

#include "errlog.h"

#include "my.h"
#include "hiredis.h"

void reg_redis(char* src_ip, char* src_mac, char* url, char* words, int size){

	redisReply *reply;
	redisContext *c = redisConnect("127.0.0.1",6379);

	if(c==NULL||c->err){
		log_error(LOG_LEVEL_ERROR,"Failed to connect redis-server -> %s",c->err);
		return;
	}

	reply = redisCommand(c,"SELECT %d",1);
	if(reply==NULL){
		log_error(LOG_LEVEL_ERROR,"%s failed.",reply->str);
		return;
	}

	// XXX: hiredis only parses individual parameters to
	// 		deliver them over redis protocol. Do not give
	// 		all the parameters at once in a format string.
	reply = redisCommand(c,"HMSET %s src_ip %s src_mac %s url %s words '%s' size %d",
							src_mac, src_ip, src_mac, url, words, size);

	if(reply->type==REDIS_REPLY_ERROR){
		char comm[256];
		sprintf(comm, "HMSET %s src_ip %s src_mac %s url %s words \"%s\" size %d",
				src_mac, src_ip, src_mac, url, words, size);
		log_error(LOG_LEVEL_ERROR,"HMSET error: %s.\n%s",reply->str,comm);
		return;
	}

}


