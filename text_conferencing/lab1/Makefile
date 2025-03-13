DEPS = packet.h user.h session.h

all: server client
server: server.c $(DEPS)
	gcc server.c -o server
client: client.c $(DEPS)
	gcc client.c -o client
clean:
	rm -f server client