CC = gcc
CFLAGS = -Wall -O2 -lGL -lGLU -lm -lSDL_image -lfftw3 `sdl-config --cflags --libs`
OBJECTS = main.o fft.o draw.o table.o serial.o

main : $(OBJECTS)
		$(CC) $(CFLAGS) $(OBJECTS) -o main

%.o : %.c
		$(CC) $(CFLAGS) -c $<

clean:
	@echo Cleaning up...
	rm -fr *.o main
	@echo Done.
