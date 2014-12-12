#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <ctype.h> // toupper
#include <errno.h>

#include "errlog.h"

#include "my.h"
#include <iconv.h>

int strremgap(char* str, const char* start, const char* end);
int get_charset_from_http(const struct list_entry* entry, char* code);
int get_charset_from_html(char* body, char* code);
void del_css_def(char* body);
void del_html_tag(char* body);
void del_brackets(char* body);
void conv_charset(char* code, char* body);
void make_symbols_blank(char* body);
void get_bag_of_words(char* body, char* freq, const int bow, const int bow_ignore_case, const int bow_min_freq_as_word, const int bow_min_word_len);
void get_src_mac(char* src_ip, char* src_mac);
void get_host_mysql(char* src_mac, char* phost);
void my_reg_size(struct client_state *csp);

char* db_user;
char* db_pass;
int bag_of_words;

/*
 * This is the main function to register http content into db
 *
 */
void my_http_content_reg(struct client_state *csp)
{

	char code[SSBUF] = {'\0'};
	if(!get_charset_from_html(csp->iob->cur,code)){
		if(!get_charset_from_http(csp->headers->first,code))
			return;
	}

	char* body = (char*)calloc(TEXTBUF,sizeof(char));
	if(body==NULL){
		log_error(LOG_LEVEL_ERROR,"Calloc failed ");
		return;
	}

	strlcpy(body,csp->iob->cur,TEXTBUF);
	del_css_def(body);
	del_html_tag(body);
	del_brackets(body);
	if(strlen(code)>0)
		conv_charset(code, body);

	make_symbols_blank(body);

	// XXX: conversion may make the length bigger due to replacing ' to \'
	char *tmp= (char*)calloc(TEXTBUF,sizeof(char));
	if(tmp==NULL){
		log_error(LOG_LEVEL_ERROR,"Calloc failed ");
		return;
	}

	char *check;
	if(csp->config->bag_of_words==1){
		check=strtok(body,"-',.");
		while(check!=NULL){
			strcat(tmp,(const char*)check);
			strcat(tmp," ");
			check=strtok(NULL,"-',.");
		}
	}else{
		check=strtok(body,"'");
		while(check!=NULL){
			strcat(tmp,(const char*)check);
			strcat(tmp,"\\\'");
			check=strtok(NULL,"'");
		}
	}
	strlcpy(body, tmp, TEXTBUF);
	free(tmp);

	char* freq= (char*)calloc(TEXTBUF,sizeof(char));
	if(freq==NULL){
		log_error(LOG_LEVEL_ERROR,"Calloc failed ");
		return;
	}

	get_bag_of_words(body,\
					 freq,\
					 (const int)csp->config->bag_of_words,\
					 (const int)csp->config->bow_ignore_case,\
					 (const int)csp->config->bow_min_freq_as_word,\
					 (const int)csp->config->bow_min_word_len);
	free(body);

	char src_mac[SBUF] = {'\0'};
	if(strcmp(csp->ip_addr_str,"127.0.0.1")==0){
		strcpy(src_mac,"00:00:00:00:00:00");
	}else{
		get_src_mac(csp->ip_addr_str,src_mac);
	}

	log_error(LOG_LEVEL_INFO,\
			  "IP %s / Source mac %s / URL %s",csp->ip_addr_str, src_mac, csp->http->host);


	// Global variables
	db_user=csp->config->database_user;
	db_pass=csp->config->database_password;
	bag_of_words=csp->config->bag_of_words;


	// FIXME:
	// - Implement memcached
	// - Make strategy more efficient and nice
	//
	switch(csp->config->database){

		// Strategy:
		// Suppress swelling # of rows by small pages
		// In 60 seconds, only larger page will be stored
		case DATABASE_MYSQL:
			log_error(LOG_LEVEL_INFO,"MySQL is selected.");
			reg_mysql(csp->ip_addr_str,
					  src_mac,
					  csp->http->host,
					  freq,
					  csp->content_length);

			break;

		// Strategy:
		// Suppress swelling # of rows by small pages
		// In 60 seconds, only larger page will be stored
		case DATABASE_MONGODB:
			log_error(LOG_LEVEL_INFO,"MongoDB is selected.");
			reg_mongodb(csp->ip_addr_str,
						src_mac,
						csp->http->host,
						freq,
						csp->content_length);

			break;

		// Strategy:
		// Simply, one mac address, one record
		// Keep overwritting along with mac address
		case DATABASE_REDIS:
			log_error(LOG_LEVEL_INFO,"Redis is selected.");
			reg_redis(csp->ip_addr_str,
					  src_mac,
					  csp->http->host,
					  freq,
					  csp->content_length);
			break;

		// Strategy:
		// Just keep inserting
		// Put words into hstore where key is term and
		// value is its freq only if bag_of_words is enabled
		case DATABASE_POSTGRESQL:
			log_error(LOG_LEVEL_INFO,"PostgreSQL is selected.");
			reg_postgresql(csp->ip_addr_str,
						   src_mac,
						   csp->http->host,
						   freq,
						   csp->content_length);

			break;

		case DATABASE_MEMCACHED:
		default:
			log_error(LOG_LEVEL_FATAL, "No database matched.");
			return;
	}

	free(freq);

}


/*
 * This is common and destructive function.
 * The gap described by start and end will be cut away from str.
 */
int cut_gap_away(char* str, const char* start, const char* end){

	char *buf;
	if( (buf = (char*)calloc(TEXTBUF,sizeof(char))) == NULL ){
		log_error(LOG_LEVEL_INFO, "cannot allocate memory");
		return -1;
	}

	char *ps0;
	if( (ps0=strstr(str,start)) != NULL ){
		strncat( buf,str,(ps0-str)/sizeof(char) );
	}else{
		free(buf);
		return 0;
	}

	char *ps1;
	if( (ps1=strstr(ps0+(strlen(start)+1)*sizeof(char),end)) != NULL){
		strcat( buf,ps1+(strlen(end))*sizeof(char) );
	}else{
		// this is not abnormal case, so just returns 0
		log_error(LOG_LEVEL_ERROR, "there is no %s in str after %s",end,start);
		free(buf);
		return 0;
	}

	strlcpy(str, buf, TEXTBUF);
	str[strlen(buf)] = '\0';

	free(buf);
	return 1;
}

int get_charset_from_http(const struct list_entry* entry, char* code)
{
	int res=0;

	char *c;
	while( entry != NULL ){
		if( strstr(entry->str,"HTTP/1.1")!=NULL ){
			if( strstr(entry->str,"200")==NULL ){
				break;
			}
		}else if( strstr(entry->str,"Content-Type:")!=NULL ){
			if( strstr(entry->str,"text/html")!=NULL ){
				if( (c=strstr(entry->str,"charset="))!=NULL ){
					strcpy( code, (const char*)(c+(strlen("charset=")*sizeof(char))) );
					res=1;
				}
			}
			break;
		}
		entry = entry->next;
	}

	return res;
}

int get_charset_from_html(char* body, char* code)
{
	char *cs1, *cs2;
	if( (cs1=strstr(body, "charset")) != NULL ){
		cs1=cs1+(strlen("charset"))*sizeof(char);

		for(;;){
			if(*cs1=='='||*cs1=='"') ++cs1;
			else break;
		}

		cs2=cs1;
		for(;;){
			if(*cs2==';'||*cs2=='"') ++cs2;
			else break;
		}

		if(cs2-cs1>0){
			strlcpy(code, cs1, SSBUF);
		}
		return 1;
	}

	return 0;

}

void del_css_def(char* body)
{
	int res;

	while((res=cut_gap_away(body, "<script", "</script>"))==1){}
	if(res==-1){
		log_error(LOG_LEVEL_FATAL, "cut_gap_away for <script> failed");
		exit(1);
	}

	while((res=cut_gap_away(body, "<style",  "</style>"))==1){}
	if(res==-1){
		log_error(LOG_LEVEL_FATAL,"cut_gap_away for <style> failed");
		exit(1);
	}
}

void del_html_tag(char* body)
{
	int res;

	while((res=cut_gap_away(body, "<",  ">"))==1){}
	if(res==-1){
		log_error(LOG_LEVEL_FATAL,"cut_gap_away for <*> failed");
		exit(1);
	}
}

void del_brackets(char* body)
{
	int res;

	while((res=cut_gap_away(body, "[",  "]"))==1){}
	if(res==-1)
		log_error(LOG_LEVEL_FATAL,"cut_gap_away for [*] failed");

	while((res=cut_gap_away(body, "{",  "}"))==1){}
	if(res==-1)
		log_error(LOG_LEVEL_FATAL,"cut_gap_away for {*} failed");

	while((res=cut_gap_away(body, "\"",  "\""))==1){}
	if(res==-1)
		log_error(LOG_LEVEL_FATAL,"cut_gap_away for {*} failed");

}

void conv_charset(char* code, char* body)
{
	// Make code word upper
	// utf-8 -> UTF-8
	char* pc;
	for(pc=code;*pc;pc++){
		if(*pc=='='){
			*pc = ' ';
		}else{
			*pc = toupper(*pc);
		}
	}

	// XXX: conversion may make the length bigger
	char *tmp= (char*)calloc(TEXTBUF,sizeof(char));
	if(tmp==NULL){
		log_error(LOG_LEVEL_ERROR,"Calloc failed ");
		return;
	}

	if(strcmp(code,"UTF-8")!=0){

		iconv_t ic;
		char* src = body;
		char* dst = tmp;
		size_t src_n = strlen(body);
		size_t dst_n = sizeof(tmp)-1;

		if( (ic=iconv_open("UTF-8", (const char*)code)) != (iconv_t)(-1) ){
			if( (size_t)(-1) == iconv( ic, &src, &src_n, &dst, &dst_n ) ){
				log_error(LOG_LEVEL_ERROR, "iconv failed %d (%s)",errno,code);
			}else{
				strlcpy(body, tmp, TEXTBUF);
			}
		}else{
			log_error(LOG_LEVEL_ERROR, "iconv_open failed %d (%s)",errno,code);
		}
		iconv_close(ic);
	}

	free(tmp);
}

void make_symbols_blank(char* body)
{
	char *tmp= (char*)calloc(TEXTBUF,sizeof(char));
	if(tmp==NULL){
		log_error(LOG_LEVEL_ERROR,"Calloc failed ");
		return;
	}

	char *check;

	check=strtok(body,SYMBOLS);
	while(check!=NULL){
		strcat(tmp,(const char*)check);
		strcat(tmp," ");
		check=strtok(NULL,SYMBOLS);
	}
	strlcpy(body, tmp, TEXTBUF);
	free(tmp);
}

void get_src_mac(char* src_ip, char* src_mac)
{
	FILE *fp;
	char buf[SBUF] = {'\0'};
	char mycmdline[SBUF] = {'\0'};

	sprintf(mycmdline,"/sbin/arp | grep %s | awk '{print $3}'",src_ip);	
	if((fp=popen(mycmdline,"r"))==NULL){
		log_error(LOG_LEVEL_FATAL, "cannot get mac addr.");	
		return;
	}

	while(fgets(buf,SBUF,fp) != NULL){
		sprintf(src_mac,"%s", buf);
	}
	pclose(fp);
	src_mac[strlen(src_mac)-1] = '\0';

}

/*
 * bag of words is the array of {word, count} such as
 * "aaa 10 bbb 5 ccc 20 ddd 1 ...".
 * A count is frequency of the word in a document.
 */
void get_bag_of_words(char* body, char* freq, const int bow, const int bow_ignore_case, const int bow_min_freq_as_word, const int bow_min_word_len)
{
	log_error(LOG_LEVEL_INFO,"bow_ignore_case %d",bow_ignore_case);
	if(bow==0){
		strlcpy(freq,body,TEXTBUF);
		return;
	}

	char words[ARRBUF][SSBUF]={};
	int freqs[ARRBUF]={};
	char *word;
	int i,cnt=0;

	word=strtok(body," ");

	while(word!=NULL){

		if(bow_min_word_len>strlen(word)){
			word=strtok(NULL," ");
			continue;
		}

		if(bow_ignore_case){
			char* pc;
			for(pc=word;*pc;pc++){
				*pc = tolower(*pc);
			}
		}

		for(i=0;i<cnt;i++){
			if(0==strcmp(words[i],word)){
				freqs[i]++;
				break;
			}
		}
		if(i==cnt){
			strlcpy(words[cnt],word,SSBUF);
			freqs[cnt]=1;
			cnt++;
		}
		word=strtok(NULL," ");
	}

	char f[SSBUF]={};
	for(i=0;i<cnt;i++){
		if(bow_min_freq_as_word<=freqs[i]){
			strcat(freq,words[i]);
			strcat(freq," ");
			sprintf(f,"%d",freqs[i]);
			strcat(freq,f);
			if(i+1<cnt) strcat(freq," ");
		}
	}
}


