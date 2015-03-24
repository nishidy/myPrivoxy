#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <malloc.h>

#include <cassandra.h>

#include "errlog.h"
#include "my.h"

void reg_cassandra(char* src_ip, char* src_mac, char* url, char* words, int size){

	CassCluster* cluster = cass_cluster_new();
	CassSession* session = cass_session_new();

	cass_cluster_set_contact_points(cluster, (const char*)src_ip);
	CassFuture* connect_future = cass_session_connect(session, cluster);

	CassError rc = cass_future_error_code(connect_future);
	if( rc != CASS_OK){
		printf("Connection failed. : %s\n",cass_error_desc(rc));
	}

	CassString query = cass_string_init("INSERT INTO myprivoxy (src_ip, src_mac, url, words, size) VALUES (?,?,?,?,?)");

	CassStatement* statement = cass_statement_new(query,5);
	cass_statement_bind_string(statement, 0, cass_string_init((const char*)src_ip));
	cass_statement_bind_string(statement, 1, cass_string_init((const char*)src_mac));
	cass_statement_bind_string(statement, 2, cass_string_init((const char*)url));
	cass_statement_bind_string(statement, 3, cass_string_init((const char*)words));
	cass_statement_bind_int32(statement, 4, size);

	CassFuture* query_future = cass_session_execute(session, statement);
	rc = cass_future_error_code(query_future);

	if( rc != CASS_OK){
		printf("Connection failed. : %s\n",cass_error_desc(rc));
	}

	cass_statement_free(statement);

	cass_future_free(connect_future);
	cass_session_free(session);
	cass_cluster_free(cluster);

}

