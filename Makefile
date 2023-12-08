compile: server client

server: server.c account.c
	gcc -Wall -g3 -fsanitize=address -pthread server.c account.c -o server $(FLAGS) $(LIBS)

client: client.c account.c
	gcc -Wall -g3 -fsanitize=address -pthread client.c account.c -o client $(FLAGS) $(LIBS)

FLAGS    = -L /lib64
LIBS     = -lusb-1.0 -lpthread

.PHONY: clean
clean:
	rm -f server client


