# Projekt:	Synchronizace procesu
# Autor:	Tomas Valek
# Login:	xvalek02
# Datum:	01.05.2011 
# 

# překladač jazyka C:
CC=gcc
# parametry překladače:
CFLAGS= -std=gnu99 -Wall -Wextra -pedantic -Werror

all: barbers

barbers: barbers.c
	$(CC) $(CFLAGS) -lrt -pthread -o $@ $<
