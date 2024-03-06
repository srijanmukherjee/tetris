CC=gcc
CFLAGS=-Wall -ggdb `sdl2-config --cflags` `pkg-config --cflags SDL2_image SDL2_mixer`
LIBS=`sdl2-config --libs` `pkg-config --libs SDL2_image SDL2_mixer`
BUILDIR=build
SRCDIR=src
EXECUTABLE=tetris

all: $(BUILDIR)/$(EXECUTABLE)

assets: assets/img/pallete.png

$(BUILDIR)/$(EXECUTABLE): $(SRCDIR)/main.c
	$(CC) $(CFLAGS) -o $(BUILDIR)/$(EXECUTABLE) $(SRCDIR)/main.c $(LIBS)

assets/img/pallete.png: assets/img/pallete.svg
	convert -background none assets/img/pallete.svg assets/img/pallete.png

clean:
	rm build/*