FLAGS=-O2 -D_DEFAULT_SOURCE -g -Wall -Wextra -pedantic -std=c99
LDFLAGS=

SRC = $(wildcard src/*.c)
HDR = $(wildcard src/*.h)
OBJ = $(SRC:src/%.c=obj/%.o)

.PHONY: all format

all: purity ignore.txt

format: $(SRC) $(HDR)
	echo $(SRC) $(HDR)
	clang-format -i --style=file $^

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
