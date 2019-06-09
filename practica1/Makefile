# Servidor HTTP
CC = gcc
CFLAGS = -Wall -Wextra -Werror -ggdb -Iinclude -std=gnu11
LDFLAGS = -Llib
LDLIBS = -lconfuse -lqsem -lpicohttpparser -pthread
DIR := ${CURDIR}

#librerias externas
LIBRARIES = lib/libconfuse.a lib/libqsem.a lib/libpicohttpparser.a

# Todos los objetos
OBJECTS = src/main.o src/tcp_server_common.o src/tcp_server_fork.o \
		  src/tcp_server_secuencial.o src/tcp_server_thread.o      	\
		  src/http.o src/config.o src/picohttpparser.o src/http_utils.o \
		  src/dir_listing.o

# Tipo de servidor TCP que va a usar
# Opciones disponibles:
#      tcp_server_secuencial
#      tcp_server_fork
#      tcp_server_thread
#      tcp_server_pftpc
TCP_SERVER = tcp_server_pftpc

all: $(LIBRARIES) http_server

http_server: src/main.o src/tcp_server_common.o src/$(TCP_SERVER).o src/http.o \
             src/config.o src/http_utils.o src/dir_listing.o
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS) 

#objetos y dependencias
src/main.o: src/main.c include/tcp_server.h include/http.h include/config.h
src/tcp_server_common.o: src/tcp_server_common.c
src/tcp_server_secuencial.o: src/tcp_server_secuencial.c include/config.h
src/tcp_server_fork.o: src/tcp_server_fork.c include/config.h
src/http_utils.o: src/http_utils.c include/picohttpparser.h include/http.h
src/http.o: src/http.c include/config.h include/picohttpparser.h
src/dir_listing.o: src/dir_listing.c include/http.h

src/config.o: src/config.c

libconfuse: lib/libconfuse.a

lib/libconfuse.a:
	cd srclib/libconfuse;                   \
	sh configure --disable-examples --silent --enable-silent-rules; make -j4;        \
	cp ./src/.libs/libconfuse.a $(DIR)/lib/;\
	cp ./src/confuse.h $(DIR)/include/;

lib/libqsem.a: srclib/qsem/qsem.o
	ar -r $@ $^

lib/libpicohttpparser.a: srclib/picohttpparser/picohttpparser.o
	ar -r $@ $^

clean:
	rm -rf src/*.o main http_server lib/*.a srclib/qsem/*.o srclib/picohttpparser/*.o

clear:
	rm -rf src/*.o main http_server srclib/qsem/*.o srclib/picohttpparser/*.o


PHONY: clean all libconfuse