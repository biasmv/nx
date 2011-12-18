CFLAGS=-O2 -DNDEBUG
PREFIX=/usr/local

nx: nx.c
	${CC} -lm -Wall -o nx nx.c ${CFLAGS}
clean:
	rm -f nx
all: nx
	
install: all
	mkdir -p ${PREFIX}/bin
	cp nx ${PREFIX}/bin/nx
	ln -sf ${PREFIX}/bin/nx ${PREFIX}/bin/10x
	ln -sf ${PREFIX}/bin/nx ${PREFIX}/bin/20x
	ln -sf ${PREFIX}/bin/nx ${PREFIX}/bin/5x