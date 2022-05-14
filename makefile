CC = gcc
CFLAGS = -Wall -Wextra -g

SRC = src/server.c \
	  src/utils.c \

OBJ = $(SRC:.c=.o)

MAIN = server

$(MAIN): $(OBJ) makefile
	$(CC) $(CFLAGS) -o $(MAIN) $(OBJ)

clean: 
	rm -f $(OBJ) $(MAIN)
