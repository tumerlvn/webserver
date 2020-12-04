all: server client
	
server: src/server.c
	gcc src/server.c -o bin/server -fsanitize=address,leak

client: src/client.c
	gcc src/client.c -o bin/client -fsanitize=address,leak

clean:
	rm bin/server bin/client
