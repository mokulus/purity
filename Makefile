FLAGS=-O2 -D_DEFAULT_SOURCE -g -Wall -Wextra -pedantic -std=c99
LDFLAGS=-lbsd

SRC = $(wildcard src/*.c)
OBJ = $(SRC:src/%.c=obj/%.o)

all: purity ignore.txt

purity: $(OBJ)
	$(CC) $(LDFLAGS) $(CFLAGS) $(FLAGS) $^ -o purity

obj/%.o: src/%.c|obj
	$(CC) $(CFLAGS) $(FLAGS) -c $< -o $@

clean:
	rm -rf purity obj

install: purity
	cp $< /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/purity

ignore.txt: ignore.def.txt
	cp $< $@

obj:
	mkdir -p $@
