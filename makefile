CC     = gcc
CFLAGS = -g -Wall -Werror
DEPS   = cr.h
H_OBJ  = cr-server.o
C_OBJ  = cr-client.o

all: cr-server cr-client

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

cr-server: $(H_OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

cr-client: $(C_OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -rf *.o cr-server cr-client
