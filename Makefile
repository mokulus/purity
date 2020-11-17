all: purity ignore.txt

purity: purity.c
	$(CC) $(CFLAGS) -O2 -D_DEFAULT_SOURCE -lbsd -g -Wall -Wextra -pedantic -std=c99 -o purity purity.c

clean:
	rm -rf purity

install: purity
	cp purity /usr/local/bin/purity

uninstall:
	rm -f /usr/local/bin/purity

ignore.txt: ignore.txt.def
	cp ignore.txt.def ingore.txt
