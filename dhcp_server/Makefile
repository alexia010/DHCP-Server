TARGET = dhcp_server	#nume executabil final

SRC = my_queue.c server.c
OBJ = $(SRC:.c=.o)

CC = gcc
CFLAGS = -Wall -g

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
