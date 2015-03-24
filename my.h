#include "project.h"
#include "miscutil.h"

#define SYMBOLS "!@#$%^&*()_=+[]{}\\|;:\"<>?/\r\t\n"
// Excluded -',. from SYMBOLS

#define TEXTBUF 1000000
#define ARRBUF  65535
#define SBUF    256
#define SSBUF   64

void my_http_content_reg(struct client_state *csp);

void reg_mysql(char* src_ip, char* src_mac, char* url, char* words, int size);
void reg_mongodb(char* src_ip, char* src_mac, char* url, char* words, int size);
void reg_redis(char* src_ip, char* src_mac, char* url, char* words, int size);
void reg_postgresql(char* src_ip, char* src_mac, char* url, char* words, int size);
void reg_cassandra(char* src_ip, char* src_mac, char* url, char* words, int size);

extern int global_pg_prepared;

extern char* db_user;
extern char* db_pass;
extern int bag_of_words;


