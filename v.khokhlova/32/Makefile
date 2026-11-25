
CC = gcc
CFLAGS = -Wall -Wextra -O2

.PHONY: all server client stop clean

all: task32_server task32_client

server: task32_server
	@./task32_server </dev/null >/dev/tty 2>&1 &
	@sleep 1

client: task32_client
	@./task32_client

stop:
	@if [ -f .task32_server.pid ]; then kill -INT `cat .task32_server.pid`; else echo "Server PID file not found."; fi

task32_server: task32_server.c
	@LIBS=""; [ "`uname -s`" = "SunOS" ] && LIBS="-lsocket -lnsl"; $(CC) $(CFLAGS) task32_server.c -o task32_server $$LIBS

task32_client: task32_client.c
	@LIBS=""; [ "`uname -s`" = "SunOS" ] && LIBS="-lsocket -lnsl"; $(CC) $(CFLAGS) task32_client.c -o task32_client $$LIBS

clean:
	rm -f task32_server task32_client .task32_server.pid /tmp/task32_vkhokhlova.sock

