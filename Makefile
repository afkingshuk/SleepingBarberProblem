# Project:	Synchronization of proceses
# Author:	Tomas Valek
# Login:	xvalek02
# Date:		01.05.2011 
# 

NAME = barbers

# compiler of C language:
CC=gcc
# params
CFLAGS= -std=gnu99 -Wall -Wextra -pedantic -Werror
CLIBS= -lpthread -lrt

all: $(NAME)

.PHONY: clean
.PHONY: pack

$(NAME): $(NAME).c
	$(CC) $(CFLAGS) -o $@ $< $(CLIBS)

clean:
	rm -f *.o $(NAME) $(NAME).zip

pack:
	zip $(NAME).zip $(NAME).c Makefile README output
