CC=gcc
CFLAGS=-Wall -iquote include/
LDFLAGS=-lSDL2 -lm

# Windows cross-build. SDL2_MINGW must point at the x86_64-w64-mingw32
# directory of an SDL2-devel-*-mingw release, which provides libSDL2.a.
WIN_CC=x86_64-w64-mingw32-gcc
WIN_LIBS=-lm -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 \
	-lole32 -loleaut32 -lshell32 -lsetupapi -lversion -luuid

all: corg

windows: corg.exe

clean:
	rm -f src/*.o
	rm -f corg corg.exe

corg:    src/organya.o src/main.o
	$(CC) $(CFLAGS) -o corg src/*.o $(LDFLAGS)

src/%.o: src/%.c include/%.h

corg.exe: src/organya.c src/main.c include/organya.h
	$(if $(SDL2_MINGW),,$(error Set SDL2_MINGW to an SDL2 MinGW dev directory))
	$(WIN_CC) $(CFLAGS) -I$(SDL2_MINGW)/include -o corg.exe \
		src/organya.c src/main.c -static -s $(SDL2_MINGW)/lib/libSDL2.a \
		$(WIN_LIBS)

.PHONY: all windows clean
