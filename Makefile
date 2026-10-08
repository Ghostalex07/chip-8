CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -pedantic -O2
SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS   := $(shell sdl2-config --libs)

BIN      = chip8
SRC      = src/main.c src/chip8.c src/display.c src/input.c src/audio.c
OBJ      = $(SRC:.c=.o)
HEADERS  = $(wildcard src/*.h)

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(SDL_LIBS)

src/%.o: src/%.c $(HEADERS)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(BIN)

.PHONY: all clean
