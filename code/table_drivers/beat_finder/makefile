CC = gcc -Wall -O2

all:
	$(CC) main.c fft.c draw.c table.c serial.c -o main -lGL -lGLU -lm -lSDL_image -lfftw3 `sdl-config --cflags --libs`

clean:
	@echo Cleaning up...
	@rm main
	@echo Done.
