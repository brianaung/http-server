CC = gcc
CFLAGS = -Wall -Wextra -pthread -g

SRC = src/server.c \
	  src/utils.c \

OBJ = $(SRC:.c=.o)

MAIN = server

$(MAIN): $(OBJ) makefile
	$(CC) $(CFLAGS) -o $(MAIN) $(OBJ)

clean: 
	rm -f $(OBJ) $(MAIN)

run:
	valgrind --leak-check=full --track-origins=yes ./server 4 3000 /home/brian/www
