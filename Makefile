CC=gcc
LIBS=-levent -lleveldb -lm

all: chunkedstorage

chunkedstorage : src/chunkedstorage.c
	$(CC) src/chunkedstorage.c libs/leveldb_api/db.c -o chunkedstorage $(LIBS)

clean :
	rm -rf chunkedstorage

dist-clean :
	rm -rf chunkedstorage
	rm -rf testdb
