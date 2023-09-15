CC = gcc
CFLAGS_server = -Wall -Wextra -lpthread -DSERVER
CFLAGS_client = -Wall -Wextra -lpthread
SRCS_server = server.c double_linked_list_opt.c function.c gobackn.c
SRCS_client = client.c gobackn.c
OBJS_server = $(SRCS_server:.c=.o)
OBJS_client = $(SRCS_client:.c=.o)
TARGET = server client

all: $(TARGET)

server: $(OBJS_server)
	$(CC) $(CFLAGS_server) $(OBJS_server) -o server 

client: $(OBJS_client)
	$(CC) $(CFLAGS_client) $(OBJS_client) -o client

%.o: %.c
	$(CC) $(CFLAGS_client) $< -c -o $@

.PHONY: clean
clean:
	-$(RM) server client $(OBJS_server) $(OBJS_client)
