CC = gcc -Wall -ansi -std=c99 -O2

all:
	$(CC) main.c draw.c table.c serial.c -o main -lGL -lGLU -lm -lSDL_image -lfftw3 `sdl-config --cflags --libs`

clean:
	@echo Cleaning up...
	@rm main
	@echo Done.
