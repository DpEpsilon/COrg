CC=gcc
CFLAGS=-Wall -iquote include/
LDFLAGS=-lSDL -lm

all: corg

clean:
	rm -f src/*.o
	rm -f corg

corg:    src/organya.o src/main.o
	$(CC) $(CFLAGS) -o corg src/*.o $(LDFLAGS)

src/%.o: src/%.c include/%.h
