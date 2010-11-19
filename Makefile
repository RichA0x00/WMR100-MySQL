LIBS = -lusb -L/usr/local/lib -lhid -lmysqlclient
CC = gcc

all: wmr100

wmr100: 
	$(CC)  wmr100.c mysql.c -o wmr100 $(LIBS) 
	
clean:
	-rm wmr100
