CC = gcc
CFLAGS = -Wall -Wextra -O2

.PHONY: all server client clean

all: task30_server task30_client

server: task30_server
	@./task30_server </dev/null >/dev/tty 2>&1 &
	@sleep 1

client: task30_client
	@./task30_client

task30_server: task30_server.c
	@LIBS=""; [ "`uname -s`" = "SunOS" ] && LIBS="-lsocket -lnsl"; $(CC) $(CFLAGS) task30_server.c -o task30_server $$LIBS

task30_client: task30_client.c
	@LIBS=""; [ "`uname -s`" = "SunOS" ] && LIBS="-lsocket -lnsl"; $(CC) $(CFLAGS) task30_client.c -o task30_client $$LIBS

clean:
	rm -f task30_server task30_client /tmp/task30_vkhokhlova.sock
