CC = gcc
CFLAGS_server = -Wall -Wextra -lpthread -DSERVER
CFLAGS_client = -Wall -Wextra -lpthread
SRCS_server = server.c double_linked_list_opt.c function.c transport_protocol.c
SRCS_client = client.c transport_protocol.c

TARGET = server client




all:
	@gcc -g -o server $(SRCS_server)  $(CFLAGS_server) 
	@gcc -g -o client $(SRCS_client) $(CFLAGS_client)

rm:
	find . -type f ! -name "*.c" ! -name "*.txt" ! -name "*.h" ! -name "Makefile" ! -name "*.out" -print0 | xargs -0 rm -v