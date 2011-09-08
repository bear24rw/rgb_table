CC = gcc
CFLAGS = -Wall -O2 -lGL -lGLU -lm -lSDL_image -lfftw3 -lftdi `pkg-config ftgl freetype2 --cflags --libs` `sdl-config --cflags --libs` -I/usr/include/freetype2/
OBJECTS = main.o fft.o draw.o table.o serial.o

main : $(OBJECTS)
		$(CC) $(CFLAGS) $(OBJECTS) -o main

%.o : %.c
		$(CC) $(CFLAGS) -c $<

clean:
	@echo Cleaning up...
	rm -fr *.o main
	@echo Done.
