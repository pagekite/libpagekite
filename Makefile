# Makefile!

OPT ?= -O3
CFLAGS ?= -std=c99 -pedantic -Wall -W $(OPT)
CLINK ?= -lm

OBJ = pkite.o pkproto.o

pkite: $(OBJ)
	$(CC) $(CFLAGS) $(CLINK) -o pkite $(OBJ)

clean:
	rm -f pkite *.o

.c.o:
	$(CC) $(CFLAGS) -c $<

