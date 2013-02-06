
CC=g++

CFLAGS=-I/usr/include/mysql -DBIG_JOINS=1  -fno-strict-aliasing  -g -L/usr/lib/x86_64-linux-gnu -lmysqlclient -lpthread -lz -lm -lrt -ldl

LDFLAGS=$(mysql_config --cflags) $(mysql_config --libs)
all: mysql.o

mysql.o: mysql_wrapper.cpp	
	$(CC) $(CFLAGS) mysql_wrapper.cpp -o a.out
