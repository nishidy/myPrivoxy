#Purpose
Privoxy is cotent-filtering proxy server.
myPrivoxy, this forked project, can store the contents (English text) that you download with some complementary information into database among your choices.
Currently, MySQL, PostgreSQL(hstore enabled), MongoDB and redis are available.
You can configure anything in config file, which means you do not even need to restart the program to change parameters and/or database.

Additionally, myPrivoxy can create bag-of-words for you so that you can quickly start to process them as part of text analysis.
You can also specify some parameters in config file for bag-of-words.

#Version
Forked from privoxy-3.0.21.

#Requirement
This project needs several libraries such as mysqlclient, pq, hiredis, mongoc and jansson.
Please see GNUmakefile.in and find what you need to install.

