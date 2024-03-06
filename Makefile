CC=gcc
CFLAGS=-Wall -ggdb `sdl2-config --cflags` `pkg-config --cflags SDL2_image SDL2_mixer SDL2_ttf` -lm
LIBS=`sdl2-config --libs` `pkg-config --libs SDL2_image SDL2_mixer SDL2_ttf`
BUILDIR=build
SRCDIR=src
EXECUTABLE=tetris

all: $(BUILDIR)/$(EXECUTABLE) assets

assets: assets/img/pallete.png assets/img/play_btn.png

$(BUILDIR)/$(EXECUTABLE): $(SRCDIR)/main.c
	$(CC) $(CFLAGS) -o $(BUILDIR)/$(EXECUTABLE) $(SRCDIR)/main.c $(LIBS)

assets/img/pallete.png: assets/img/pallete.svg
	convert -background none assets/img/pallete.svg assets/img/pallete.png

assets/img/play_btn.png: assets/img/play_btn.svg
	convert -background none assets/img/play_btn.svg assets/img/play_btn.png

clean:
	rm build/*