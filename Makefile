CC=gcc
CFLAGS=-O2 -DNDEBUG
PREFIX=/usr/local
nx:
	${CC} -o nx nx.c ${CFLAGS}

all: nx
	
install: all
	cp nx ${PREFIX}/bin/nx
	ln -sf ${PREFIX}/bin/nx ${PREFIX}/bin/10x
	ln -sf ${PREFIX}/bin/nx ${PREFIX}/bin/20x
	ln -sf ${PREFIX}/bin/nx ${PREFIX}/bin/5x