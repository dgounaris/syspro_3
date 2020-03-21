CC = gcc
CFLAGS = -g -O3 -lpthread
SEROBJS = dropbox_server.o msgs.o socket_utils.o ds_structs.o
CLIOBJS = dropbox_client.o msgs.o socket_utils.o dc_structs.o dirutils.o

default: dropbox_server

dropbox_server: $(SEROBJS)
	$(CC) $(SEROBJS) $(CFLAGS) -o dropbox_server

dropbox_client: $(CLIOBJS)
	$(CC) $(CLIOBJS) $(CFLAGS) -o dropbox_client

dropbox_server.o: dropbox_server.c
	$(CC) $(CFLAGS) -c dropbox_server.c
	
dropbox_client.o: dropbox_server.c
	$(CC) $(CFLAGS) -c dropbox_client.c

msgs.o: msgs.c msgs.h
	$(CC) $(CFLAGS) -c msgs.c

socket_utils.o: socket_utils.c socket_utils.h
	$(CC) $(CFLAGS) -c socket_utils.c

ds_structs.o: ds_structs.c ds_structs.h
	$(CC) $(CFLAGS) -c ds_structs.c

dc_structs.o: dc_structs.c dc_structs.h
	$(CC) $(CFLAGS) -c dc_structs.c

dirutils.o: dirutils.c dirutils.h
	${CC} $(CFLAGS) -c dirutils.c

clean:
	rm -f count *.o *~
