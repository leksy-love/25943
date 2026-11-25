
CC = gcc
CFLAGS = -Wall -Wextra -O2

.PHONY: all server client stop clean

all: task31_server task31_client

server: task31_server
	@./task31_server </dev/null >/dev/tty 2>&1 &
	@sleep 1

client: task31_client
	@./task31_client

stop:
	@if [ -f .task31_server.pid ]; then kill -INT `cat .task31_server.pid`; else echo "Server PID file not found."; fi

task31_server: task31_server.c
	@LIBS=""; [ "`uname -s`" = "SunOS" ] && LIBS="-lsocket -lnsl"; $(CC) $(CFLAGS) task31_server.c -o task31_server $$LIBS

task31_client: task31_client.c
	@LIBS=""; [ "`uname -s`" = "SunOS" ] && LIBS="-lsocket -lnsl"; $(CC) $(CFLAGS) task31_client.c -o task31_client $$LIBS

clean:
	rm -f task31_server task31_client .task31_server.pid /tmp/task31_vkhokhlova.sock

