# main:main.c
# 	gcc main.c sum.c -o main


# clean:
# 	rm main

CC=gcc
CFLAGS=-g  # Asigură-te că ai -g aici

LIBS = -lsqlite3

# Cauta toate fișierele .c în directorul curent
SOURCES=$(wildcard *.c)
EXECUTABLE=main

all: $(EXECUTABLE)

$(EXECUTABLE): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LIBS)

clean:
	rm -f $(EXECUTABLE) *.o

# all: gcc  queue.c logger.c dhcp_server.c dhcp_options.c main.c -o main -lsqlite3